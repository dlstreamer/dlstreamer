# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import os
import argparse
import traceback
import glob
import shutil
import os

import case_parser
from gst_pipeline_launcher import GstPipelineLauncher
from gt_comparators.base_gt_comparator import GTComparator, RuntimeTestCase, TestType
from gt_comparators.video_gt_comparator import create_video_gt_comparator
from gt_comparators.audio_gt_comparator import AudioGTComparator
from gt_comparators.benchmark_gt_comparator import BenchmarkComparator
from gt_comparators.watermark_test import HistogramComparator
from gt_comparators.video_comparators.object_classification_comparator import CheckLevel
from junit_xml_reporter import JunitXmlReporter
from utils import create_logger
from tqdm import tqdm
from config_keys import *
from xlsx_reporter import XlsxReporter
from functools import reduce
from tabulate import tabulate
from xlsx_benchmark_reporter import XlsxBenchmarkReporter


def create_arg_parser():
    parser = argparse.ArgumentParser()
    # TODO: add path to models
    parser.add_argument('-c', '--test-suite-configs', nargs="+", required=True,
                        help="Path(s) to configuration file of the test suite")
    parser.add_argument('--test-set', nargs='+', type=str,
                        help="Test set names. If the parameter is not set, all test cases in the configuration are run")
    parser.add_argument('-l', '--log-level', type=str, default='INFO',
                        help="This argument defines the logging level")
    parser.add_argument('-f', '--force', action='store_true', default=False,
                        help="Continue collecting statistics in case of a failed case")
    parser.add_argument('-x', '--xml-report', type=str, default=None,
                        help="Path to generate the xml report")
    parser.add_argument('-t', '--timeout', type=int, default=600,
                        help="Test case timeout in seconds")
    parser.add_argument('-it', '--iou_thr', type=float, default=0.9,
                        help="IOU threshold")
    parser.add_argument('-at', '--a_thr', type=float, default=8e-2,
                        help="Absolute difference threshold")
    parser.add_argument('-rt', '--r_thr', type=float, default=8e-2,
                        help="Relative difference threshold")
    parser.add_argument('-pt', '--p_thr', type=float, default=0,
                        help="Probability threshold")
    parser.add_argument('--edistance_thr', type=float, default=0.01,
                        help="Euclidean distance threshold for raw tensor comparsion. 0 = disable")
    parser.add_argument('-cl', '--check_level', type=CheckLevel, choices=list(CheckLevel),
                        default=CheckLevel.full, help="Check level")
    parser.add_argument('-et', '--error_thr', type=float, default=0.1,
                        help="Error threshold")
    parser.add_argument('-lct', '--low_conf_thr', type=float, default=0.55,
                        help="Max allowed confidence for bbox in first json to not have a pair bbox in second json")
    parser.add_argument('--disable_tqdm', default=False, action='store_true', help="Disable progress bar")
    parser.add_argument('--tags', nargs='+', default=[], help='start test case with non-empty tags intersection, '
                                                              'run all test cases by default')
    parser.add_argument('--test_sets', nargs='+', default=None, help='')
    parser.add_argument('--features', nargs='+', default=[], help='some tests require list of features to start, '
                                                                  'this option describe available features')
    parser.add_argument('--gt_specific', default="", help='specific gt sub-folder for different platforms, '
                                                          'will be joined to dataset.groundtruth.base ')
    parser.add_argument('--xlsx-report', default=None, help='Filepath for xlsx report')
    parser.add_argument('--no-summary', action='store_true', help='Disables print of summary at end of run')
    parser.add_argument('--lbl_max_err_thr', type=float, default=0.05,
                        help="Max meta object labels variance threshold")
    parser.add_argument('--output-type', type=str, choices=['json', 'file'], default='json', help='Type of output: json or file')
    parser.add_argument('--results-path', type=str, required=True, help='Path to folder for tests results')
    return parser.parse_args()


def create_gt_comparator(args, test_case: RuntimeTestCase, logger):
    comp_type = GTComparator(str(test_case.input[case_parser.COMPARATOR_FIELD]))
    if comp_type == GTComparator.video:
        return create_video_gt_comparator(test_case, args, logger)
    elif comp_type == GTComparator.audio:
        return AudioGTComparator(test_case, logger=logger)
    elif comp_type == GTComparator.watermark:
        return HistogramComparator(test_case, logger=logger)
    elif comp_type == GTComparator.performance:
        return BenchmarkComparator(test_case, logger=logger)
    raise RuntimeError("Unknown type of ground truth comparison")


def run_test_case(test_case: RuntimeTestCase, args, logger, timeout_in_sec: int = None):
    logger.warning(f"Running test case: {test_case.name}")  # Logging test name
    if args.output_type == 'json':
        gt_comparator = create_gt_comparator(args, test_case, logger)
        GstPipelineLauncher(logger=logger).launch(test_case, timeout_in_sec)
        gt_comparator.pipeline_results_processing()
        if not test_case.result.has_error:
            gt_comparator.compare()

    elif args.output_type == 'file':
        GstPipelineLauncher(logger=logger).launch(test_case, timeout_in_sec)
        if test_case.result.has_error:
            logger.error(f"Pipeline error for {test_case.name} .")
            mp4_files = glob.glob("*.mp4")
            for mp4_file in mp4_files:
                os.remove(mp4_file)
        else:
            mp4_files = glob.glob("*.mp4")
            for mp4_file in mp4_files:
                shutil.copy(mp4_file, args.results_path)
                os.remove(mp4_file)
                copied_file_path = os.path.join(args.results_path, os.path.basename(mp4_file))
                if os.path.exists(copied_file_path):
                    logger.info(f"Output file {copied_file_path} for {test_case.name} exists.")
                else:
                    logger.error(f"No output file with .mp4 extension found for {test_case.name} .")
                    test_case.result.add_error(f"No output file with .mp4 extension found {test_case.name}.")


def print_summary(test_sets: list, logger):
    total: int = 0
    failed: int = 0
    for test_set in test_sets:
        for _, test_cases in test_set.items():
            total += len(test_cases)
            failed += reduce(lambda acc, tcase: acc + 1 if tcase.result.has_error else acc, test_cases, 0)

    passed = total - failed
    pass_rate = passed / total * 100
    summary_table = tabulate([['Total', total], ['Passed', passed], [
                             'Failed', failed], ['Pass rate', f'{pass_rate}%']])

    logger.info(f"\nTESTS SUMMARY\n{summary_table}")


def main():
    args = create_arg_parser()
    logger = create_logger("regression test", args.log_level)
    parser_instance = case_parser.CaseParser(args.features, args.tags, logger=logger)

    test_sets_list_to_report = []
    logger.info(f"Configs:\n{args.test_suite_configs}")
    running_benchmark = False
    for test_suite_config in args.test_suite_configs:
        logger.info(f"Running {test_suite_config} suite...")
        test_sets_list = parser_instance.parse(test_suite_config, args.test_sets)
        test_sets_list_to_report += test_sets_list

        for test_set in tqdm(test_sets_list, total=len(test_sets_list), desc="Test sets processing",
                             disable=args.disable_tqdm):
            test_set_name, test_cases = next(iter(test_set.items()))
            for test_case in tqdm(test_cases, total=len(test_cases), desc="Test cases processing", leave=False,
                                  disable=args.disable_tqdm):
                if test_case.input[TEST_TYPE] == TestType.benchmark_performance:
                    running_benchmark = True
                try:
                    if not test_case.result.has_error:
                        test_case.gt_specific = args.gt_specific
                        run_test_case(test_case, args, logger, args.timeout)
                except Exception as exc:
                    traceback.print_exc()
                    test_case.result.add_error(str(exc))

    if args.no_summary != True:
        print_summary(test_sets_list_to_report, logger)

    if args.xlsx_report:
        if running_benchmark:
            xlsx_reporter = XlsxBenchmarkReporter(args.xlsx_report, logger=logger)
        else:
            xlsx_reporter = XlsxReporter(args.xlsx_report, logger=logger)
        xlsx_reporter.report(test_sets_list_to_report)

    if args.xml_report:
        junit_xml_reporter = JunitXmlReporter(logger=logger)
        junit_xml_reporter.report(test_sets_list_to_report)
        junit_xml_reporter.flush(args.xml_report)


if __name__ == "__main__":
    main()
