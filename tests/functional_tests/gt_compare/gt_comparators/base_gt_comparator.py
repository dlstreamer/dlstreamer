# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import os
import json
import abc
import logging
import re
import shutil

from enum import Enum
from dataclasses import dataclass, field
from pipeline_test.regression_test.config_keys import *


# Keys for additional information in CaseResult
class CaseResultInfo(Enum):
    VIDEO_STATS = 'VIDEO_STATS'


@dataclass
class CaseResult:
    _logger: logging.Logger
    has_error: bool = False
    test_error: str = ""
    stderr: str = ""
    stdout: str = ""
    duration: int = 0
    fps: float = None
    fps_target: float = 0
    fps_pass_threshold: float = 0

    # Stores any addional objects for reporting
    additional_info: dict = field(default_factory=dict)

    def add_info(self, key: CaseResultInfo, info):
        self.additional_info[key] = info

    def get_info(self, key: CaseResultInfo):
        return self.additional_info.get(key)

    def add_error(self, err_msg: str):
        log_with_level = self._logger.debug
        if not self.has_error:  # first ever error of current test case defines overall error state. Other errors can be found in debug log
            self.has_error = True
            self.test_error = err_msg
            log_with_level = self._logger.error

        log_with_level(err_msg)


@dataclass
class RuntimeTestCase:
    input: dict
    name: str
    result: CaseResult
    gt_specific: str = ""


class GTComparator(str, Enum):
    video = "video"
    audio = "audio"
    watermark = "watermark"
    performance = "performance"

    def __str__(self):
        return self.value


class TestType(str, Enum):
    sample = "sample"
    pipeline = "pipeline"
    benchmark_performance = "benchmark_performance"

    def __str__(self):
        return self.value


def extract_gt_path_from_pipeline_cmd(pipeline_cmd: str) -> str:
    metapublish_substr = re.search("! gvametapublish(.+?)!", pipeline_cmd)
    if not metapublish_substr:
        raise KeyError("The 'gvametapublish' element not found in pipeline template")
    metapublish_substr = re.search("file-path=(.+?) ", metapublish_substr.group(1))
    return metapublish_substr.group(1)


def _get_sample_tmp_output(test_input):
    return os.path.abspath(os.path.join(test_input[EXE_DIR], "output.json"))


class BaseGTComparator:
    def __init__(self, test_case: RuntimeTestCase, logger: logging.Logger = None):
        self._logger = logger if logger else logging.getLogger()
        self._test_case = test_case
        self._init_gt_path()
        self._init_prediction_path()

    def _extract_data_from_json_file(self, file_path: str) -> dict:
        if not os.path.exists(file_path):
            raise FileExistsError("The {} path to json file not found".format(file_path))
        with open(file_path, 'r') as gt_file:
            # last line is empty and last symbol of each line is '\n'
            target_parsed = [json.loads(line[:-1])
                             for line in gt_file.readlines()[:-1]]
        return target_parsed

    def _init_gt_path(self):
        keys_for_check = [GT_BASE_FOLDER_FIELD, GT_FILE_NAME_FIELD]
        keys_intersect = list(set(keys_for_check) & set(self._test_case.input))
        if len(keys_intersect) != len(keys_for_check):
            not_found_keys = set(keys_for_check) - set(keys_intersect)
            raise KeyError(
                "The required key/keys '{}' was/were not found. Looks like parsing config file went wrong".format(
                    not_found_keys))
        gt_folder = self._test_case.input[GT_BASE_FOLDER_FIELD]
        gt_path = os.path.join(gt_folder, self._test_case.gt_specific, self._test_case.input[GT_FILE_NAME_FIELD])
        if not os.path.exists(gt_path):
            gt_path = os.path.join(gt_folder, self._test_case.input[GT_FILE_NAME_FIELD])
            if not os.path.exists(gt_path):
                raise FileNotFoundError("Referenced GT file not found: {}".format(gt_path))
        self._gt_path = gt_path

    def _init_prediction_path(self):
        if self._test_case.input[TEST_TYPE] == TestType.sample or self._test_case.input[TEST_TYPE] == TestType.benchmark_performance:
            test_prediction_path = os.path.abspath(os.path.join(
                self._test_case.input[ARTIFACTS_PATH_FIELD], self._test_case.input[GT_FILE_NAME_FIELD]))
        else:
            pipeline_cmd = self._test_case.input[PIPELINE_CMD_FIELD]
            test_prediction_path = extract_gt_path_from_pipeline_cmd(pipeline_cmd)
        self._test_case.input[PREDICTION_PATH_FIELD] = test_prediction_path
        self._pred_path = test_prediction_path
        self._prepare_prediction_folder()

    def _prepare_prediction_folder(self):
        test_prediction_folder = re.search("(.*)/.*.json", self._pred_path).group(1)
        if not os.path.exists(test_prediction_folder) and not os.path.islink(test_prediction_folder):
            self._logger.info("Directory for meta publishing was not found. Create it myself...")
            self._logger.info(test_prediction_folder)
            os.mkdir(test_prediction_folder)
        # to prevent mixing results from different pipelines
        if os.path.exists(self._pred_path):
            self._logger.debug("Removed old prediction file")
            os.remove(self._pred_path)

        if self._test_case.input[TEST_TYPE] == TestType.sample and os.path.exists(_get_sample_tmp_output(self._test_case.input)):
            os.remove(_get_sample_tmp_output(self._test_case.input))

    @abc.abstractmethod
    def _compare_internal(self, origin_gt_path: str, dumped_gt_path: str):
        pass

    def pipeline_results_processing(self):
        if self._test_case.input[TEST_TYPE] == TestType.sample:
            shutil.copyfile(_get_sample_tmp_output(self._test_case.input), self._test_case.input[PREDICTION_PATH_FIELD])
        res = re.search(r"FpsCounter\(overall .*sec\): total=(\d+\.\d+) fps", self._test_case.result.stdout)
        if res:
            self._test_case.result.fps = res.group(1)

    def compare(self):
        return self._compare_internal(self._gt_path, self._pred_path)
