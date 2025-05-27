# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import os
import logging

from junit_xml import TestSuite, TestCase
from config_keys import *
from gt_comparators.base_gt_comparator import CaseResult, RuntimeTestCase


class JunitXmlReporter():
    def __init__(self, logger=None):
        self._test_suites = list()
        self._logger = logger if logger is not None else logging

    def report(self, test_sets: list):
        for test_set in test_sets:
            test_set_name, test_cases = test_set.popitem()
            ts = TestSuite(test_set_name, list())
            for test_case_id, test_case in enumerate(test_cases):
                stdout = test_case.input.get(PIPELINE_CMD_FIELD, "") + "\n" + test_case.result.stdout
                test_case_name = test_case.input.get(TC_NAME_FIELD, "test_case_{}".format(test_case_id))
                tc = TestCase(
                    test_case_name,
                    stdout=stdout,
                    stderr=test_case.result.stderr,
                    elapsed_sec=test_case.result.duration / 1000
                )
                if test_case.result.has_error:
                    if test_case.result.test_error:
                        tc.add_failure_info(test_case.result.test_error)
                    else:
                        tc.add_failure_info(test_case.result.stderr)

                ts.test_cases.append(tc)

            self._test_suites.append(ts)

    def flush(self, file_path: str):
        directory = os.path.dirname(file_path)
        if directory and not os.path.exists(directory):
            os.makedirs(directory)

        with open(file_path, 'w') as f:
            TestSuite.to_file(f, self._test_suites)

        self._logger.info("XML report generated: {}".format(file_path))
        self._test_suites.clear()
