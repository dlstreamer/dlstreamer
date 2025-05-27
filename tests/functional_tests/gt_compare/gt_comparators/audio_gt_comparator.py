# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import json
import os
import logging

from gt_comparators.base_gt_comparator import BaseGTComparator, RuntimeTestCase


class AudioGTComparator(BaseGTComparator):
    def __init__(self, test_case: RuntimeTestCase, logger: logging.Logger = None):
        super().__init__(test_case, logger if logger else logging.getLogger())

    def _compare_internal(self, origin_gt_path: str, dumped_gt_path: str):
        if not os.path.exists(origin_gt_path) or not os.path.exists(dumped_gt_path):
            raise FileExistsError(
                "Path to ground truth json file not found\nPossible invalid paths:\n{}\n{}".format(
                    origin_gt_path, dumped_gt_path))

        test_results = self._extract_data_from_json_file(dumped_gt_path)
        target_gt = self._extract_data_from_json_file(origin_gt_path)

        if len(target_gt) != len(test_results):
            err_msg = "Number of processed frames doesn't match: {} /// {}".format(
                len(target_gt), len(test_results))
            self._test_case.result.add_error(err_msg)
            return

        for target_frame, resulting_frame in zip(target_gt, test_results):
            if target_frame != resulting_frame:
                err_msg = "Result and target frames do not match: {} /// {}".format(
                    json.dumps(target_gt), json.dumps(test_results))
                self._test_case.result.add_error(err_msg)
                return

        return
