# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import json
import logging
import string
import os
from typing import List
from model_explorer import ModelExplorer
from model_proc_explorer import ModelProcExplorer
from labels_explorer import LabelsExplorer
from case_generator import CaseGenerator
from gt_compare.utils import get_platform_info
from gt_compare.gt_comparators.base_gt_comparator import TestType, GTComparator, CaseResult, RuntimeTestCase
from config_keys import *


def get_fixed_relative_path(relative_path):
    regression_tests_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    possible_base_folders = [os.environ.get("CONFIG_RELATIVE_PATH", ""),
                             regression_tests_dir, os.path.join(regression_tests_dir, "..")]

    gt_folder = ''
    for base_folder in possible_base_folders:
        possible_gt_folder = os.path.abspath(os.path.join(base_folder, relative_path))
        if os.path.exists(possible_gt_folder):
            gt_folder = possible_gt_folder
            break

    if not gt_folder:
        raise FileNotFoundError(f"Unable to find Ground truth folder using '{relative_path}' \
            relative path. Please check config properties")

    return gt_folder


class DotNameFormatter(string.Formatter):
    def get_field(self, field_name, args, kwargs):
        return self.get_value(field_name, args, kwargs), field_name


class CaseParser:
    def __init__(self, features: list, tags: list, logger: logging.Logger = None):
        self._logger = logger if logger else logging.getLogger()
        self._model_explorer = ModelExplorer(logger)
        self._model_proc_explorer = ModelProcExplorer(logger)
        self._labels_explorer = LabelsExplorer(logger)
        self._global_ts_properties = None
        self._tags_set = set(tags)
        self._available_features = set(features)

    def get_config_global_parameters(self):
        if self._global_ts_properties is None:
            raise RuntimeError("Unable to find global settings for test suites. \
                                Need to parse a config file first.")
        return self._global_ts_properties

    def parse(self, config_path: str, ts_to_run: list = None, video_dir: str = "", sample_dir: str = "") -> list:
        with open(config_path, encoding='utf-8') as json_file:
            test_suite = json.load(json_file)
        if TEST_SETS_FIELD not in test_suite.keys():
            raise ImportError("The '{}' field was not found in config json file. This filed is required".format(
                TEST_SETS_FIELD))

        test_sets = test_suite[TEST_SETS_FIELD]
        if not isinstance(test_sets, dict):
            raise ImportError("The '{}' must be of dictionary type".format(TEST_SETS_FIELD))

        self._global_ts_properties = test_suite.get(TEST_SET_PROPS_FIELD, dict())
        if not isinstance(self._global_ts_properties, dict):
            raise ImportError("The '{}' must be of dictionary type".format(TEST_SET_PROPS_FIELD))

        self._global_ts_properties[COMPARATOR_FIELD] = self._global_ts_properties.get(
            COMPARATOR_FIELD, GTComparator.video)
        self._global_ts_properties[TEST_TYPE] = self._global_ts_properties.get(TEST_TYPE, TestType.pipeline)

        if video_dir:
            self._global_ts_properties[VIDEO_DIR] = video_dir

        if sample_dir:
            self._global_ts_properties[SAMPLE_DIR] = sample_dir

        relative_path_fields = [ATTACHROI_DIR_FIELD, GT_BASE_FOLDER_FIELD]
        for config_field in relative_path_fields:
            if config_field not in self._global_ts_properties:
                continue
            self._global_ts_properties[config_field] = get_fixed_relative_path(
                self._global_ts_properties[config_field])

        test_sets_list = list()
        ts_to_run = ts_to_run if ts_to_run else test_sets.keys()
        for test_set_name in ts_to_run:
            test_set = test_sets.get(test_set_name, None)
            if not test_set:
                raise KeyError("The {} test set not found in {} json file".format(test_set_name, config_path))
            if not isinstance(test_set, (list, dict)):
                continue
            test_set_dict = dict()
            test_set_dict[test_set_name] = list()
            for test_case in CaseGenerator.generate(test_set):
                test_case_data = {**self._global_ts_properties, **test_case}
                test_case_name = test_case_data.get('name', 'default_test_case_name')  # set default name if there is no name
                test_case_data['name'] = test_case_name
                case_result = CaseResult(self._logger)
                test_case = RuntimeTestCase(input=test_case_data, name=test_case_name, result=case_result)
                platforms = test_case.input.get('platforms', [])
                platforms = [platforms] if not isinstance(platforms, list) else platforms
                if platforms and not self._is_platform_applicable(platforms):
                    continue
                if not self._is_tags_applicable(test_case.input.get('tags', [])):
                    continue
                if not self._is_features_available(test_case.input.get('features', [])):
                    continue

                if test_case.input[COMPARATOR_FIELD] != GTComparator.watermark and (test_case.input[TEST_TYPE] == TestType.pipeline or test_case.input[TEST_TYPE] == TestType.benchmark_performance):
                    try:
                        self._model_explorer.generate_paths_ip(test_case.input)
                        self._model_proc_explorer.generate_paths_ip(test_case.input)
                        self._labels_explorer.generate_paths_ip(test_case.input)
                    except Exception as e:
                        self._logger.error(e)
                        test_case.result.has_error = True
                        test_case.result.test_error = str(e)
                        test_set_dict[test_set_name].append(test_case)
                        continue
                self._generate_value_from_template(test_case.input, GT_FILE_NAME_TEMPLATE, GT_FILE_NAME_FIELD, False)
                self._generate_value_from_template(test_case.input, OUT_VIDEO_TEMPLATE, OUT_VIDEO, False)
                self._generate_value_from_template(test_case.input, EXE_DIR_TEMPLATE, EXE_DIR, False)
                test_case.input[EXE_DIR] = test_case.input.get(EXE_DIR, ".")
                self._generate_test_name(test_case.input)
                if test_case.input[TEST_TYPE] == TestType.pipeline:
                    self._generate_value_from_template(test_case.input, PIPELINE_TEMPLATE_FIELD, PIPELINE_CMD_FIELD, True)
                elif test_case.input[TEST_TYPE] == TestType.sample:
                    self._generate_value_from_template(test_case.input, SAMPLE_COMMAND_TEMPLATE, PIPELINE_CMD_FIELD, True)
                elif test_case.input[TEST_TYPE] == TestType.benchmark_performance:
                    self._generate_value_from_template(test_case.input, BENCHMARK_PERFORMANCE_COMMAND_TEMPLATE, PIPELINE_CMD_FIELD, True)
                    self._generate_value_from_template(test_case.input, BENCHMARK_PERFORMANCE_FPS_FPI, BENCHMARK_PERFORMANCE_FPS_FPI, True)
                    self._generate_value_from_template(test_case.input, BENCHMARK_PERFORMANCE_FPS_PASS_THRESHOLD, BENCHMARK_PERFORMANCE_FPS_PASS_THRESHOLD, False)
                else:
                    raise ValueError("'{}' - unknown test type in CaseParser".format(test_case.input[TEST_TYPE]))
                test_set_dict[test_set_name].append(test_case)

                self._logger.debug("{} is generated.".format(test_case.input[PIPELINE_CMD_FIELD]))
                if COMPARATOR_FIELD in test_case.input:
                    self._logger.debug("Ground truth comparator type is \"{}\".".format(
                        test_case.input[COMPARATOR_FIELD]))

            test_sets_list.append(test_set_dict)

        if (len(self._model_explorer.missed_models)) != 0:
            self._logger.warning("Some models are missing: \n%s", '\n'.join(self._model_explorer.missed_models))
        return test_sets_list

    def _is_tags_applicable(self, tags: list) -> bool:
        if len(self._tags_set) == 0:
            return True
        return len(set(tags).intersection(self._tags_set)) != 0

    def _is_features_available(self, features: list) -> bool:
        required_features = set(features)
        return len(set(required_features).intersection(self._available_features)) == len(required_features)

    def _is_platform_applicable(self, platforms: list) -> bool:
        platform_info = get_platform_info()
        result = True

        for platform in platforms:
            found = False
            for info in platform_info:
                if platform.lower() in info.lower():
                    found = True
                    break

            if not found:
                result = False
                break

        return result

    def _generate_test_name(self, test_case: dict):
        tc_name = self._generate_value_from_template(test_case, TC_NAME_TEMPLATE_FIELD, TC_NAME_FIELD, False)
        chars_to_remove = {'(', ')', ':'}
        tc_name = ''.join([c for c in tc_name if c not in chars_to_remove])
        if tc_name != "":
            test_case[TC_NAME_FIELD] = tc_name

    def _generate_value_from_template(self, test_case: dict, template_key, value_key, required) -> str:
        template = test_case.get(template_key, "")
        if not template:
            if required:
                raise ValueError("'{}' field does not defined".format(template_key))
            else:
                return ""
        value = DotNameFormatter().vformat(template, [], test_case)
        test_case[value_key] = value
        return value
