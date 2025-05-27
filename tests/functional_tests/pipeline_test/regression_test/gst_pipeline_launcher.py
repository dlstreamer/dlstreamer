# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import logging
import subprocess
import os

from gt_comparators.base_gt_comparator import CaseResult, RuntimeTestCase, TestType
from config_keys import *
from utils import get_unix_time_millsec



class GstPipelineLauncher:
    def __init__(self, logger: logging.Logger = None):
        self._logger = logger if logger else logging.getLogger()
        self._log_dir = os.path.join("/", "tmp", "regression_test")
        os.makedirs(self._log_dir, exist_ok=True)
        self._log_file = None
        self._popen_proc = None

    def launch(self, test_case: RuntimeTestCase, timeout_in_sec: int = None) -> CaseResult:
        test_case_name = test_case.input.get(TC_NAME_FIELD, "regression_test")
        log_file_path = os.path.join(self._log_dir, "{}.log".format(test_case_name))
        self._log_file = open(log_file_path, 'a')
        pipeline_cmd = []
        if test_case.input[TEST_TYPE] == TestType.pipeline or test_case.input[TEST_TYPE] == TestType.benchmark_performance:
            pipeline_cmd.append("gst-launch-1.0")
            pipeline_cmd.extend(test_case.input[PIPELINE_CMD_FIELD].split(' '))
            self.cwd = "."
        elif test_case.input[TEST_TYPE] == TestType.sample:
            pipeline_cmd.extend(test_case.input[PIPELINE_CMD_FIELD].split(','))
            self.cwd = test_case.input[EXE_DIR]
        else:
            raise ValueError("'{}' - unknown test type in CaseParser".format(test_case.input[TEST_TYPE]))

        os.environ['GST_DEBUG_NO_COLOR'] = '1'
        os.environ['XDG_RUNTIME_DIR'] = ''
        os.environ['PYTHONUNBUFFERED'] = '1'
        additional_env = test_case.input.get("env", {})
        self.case_result = test_case.result

        self._logger.info("{} Run: {}".format(test_case_name, " ".join(pipeline_cmd)))

        start_timestamp = get_unix_time_millsec()
        self._start_join(pipeline_cmd, {**os.environ.copy(), **additional_env}, timeout_in_sec)
        self.case_result.duration = get_unix_time_millsec() - start_timestamp

    def _start_join(self, process_cmd: list, env: dict, timeout: int):
        self._popen_proc = subprocess.Popen(process_cmd, env=env, stdout=subprocess.PIPE,
                                            stderr=subprocess.PIPE, preexec_fn=os.setsid, cwd=self.cwd)
        self._logger.debug("Target subprocess PID: {}".format(self._popen_proc.pid))
        try:
            self.__communicate(timeout)

            return_code = abs(self._popen_proc.returncode)
            exit_code_msg = "Process return code {}".format(return_code)
            if return_code == 0:
                logger_with_level = self._logger.debug
            else:
                logger_with_level = self._logger.error
                self.case_result.add_error(exit_code_msg + self.case_result.stderr)
            logger_with_level(self.case_result.stdout)
        except subprocess.TimeoutExpired:
            self._popen_proc.kill()
            self.__communicate()
            timeout_message_error = "Process time limit is reached. Timeout: {}\nStd out: {}\nStd err: {}".\
                                    format(timeout, self.case_result.stdout, self.case_result.stderr)
            self.case_result.add_error(timeout_message_error)
        except KeyboardInterrupt:
            self._popen_proc.kill()
            self.case_result.add_error("There was a 'KeyboardInterrupt' exception")

    def __communicate(self, timeout: int = None):
        process_stdout, process_stderr = self._popen_proc.communicate(timeout=timeout)
        self.case_result.stdout = process_stdout.decode('utf-8')
        self.case_result.stderr = process_stderr.decode('utf-8')
        self.__dump_stdout_to_file()

    def __dump_stdout_to_file(self):
        if self._log_file:
            output = "stdout:\n{}\nstderr:\n{}\n".format(self.case_result.stdout, self.case_result.stderr)
            self._log_file.write("\n")
            self._log_file.write(output)
            self._log_file.flush()
            self._log_file.close()

    def close_file(self):
        if self._log_file:
            self._log_file.close()
            self._log_file = None

    def __del__(self):
        self.close_file()
