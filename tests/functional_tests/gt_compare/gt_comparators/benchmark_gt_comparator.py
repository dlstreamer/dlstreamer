# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import logging
from config_keys import *
from video_gt_comparator import VideoCompareStats
from base_gt_comparator import BaseGTComparator, RuntimeTestCase, CaseResultInfo, extract_gt_path_from_pipeline_cmd


DEFAUL_FPS_PASS_PERCENTAGE = 100


class BenchmarkComparator(BaseGTComparator):
    def __init__(self, test_case: RuntimeTestCase, logger: logging.Logger = None):
        super().__init__(test_case, logger if logger else logging.getLogger())
        self._logger = logger if logger else logging.getLogger()
        self._test_case = test_case

    def _init_prediction_path(self):
        self._pred_path = extract_gt_path_from_pipeline_cmd(self._test_case.input[PIPELINE_CMD_FIELD])
        super()._prepare_prediction_folder()

    def _init_gt_path(self):
        self._gt_path = "" # No GT file for benchmarking performance tests

    def _get_target_fps(self):
        return round(float(self._test_case.input[BENCHMARK_PERFORMANCE_FPS_FPI]), 1)

    def _get_achieved_fps(self):
        return round(float(self._test_case.result.fps), 1)

    def _get_pass_fps_threshold_percent(self):
        try:
            return int(self._test_case.input[BENCHMARK_PERFORMANCE_FPS_PASS_THRESHOLD])
        except:
            return DEFAUL_FPS_PASS_PERCENTAGE

    def _get_pass_fps_threshold(self):
        return round((self._get_pass_fps_threshold_percent() / 100) * self._get_target_fps(), 1)

    def _save_fps_results(self):
        self._test_case.result.fps_target = self._get_target_fps()
        self._test_case.result.fps_pass_threshold = self._get_pass_fps_threshold()

    def _save_inference_results(self, prediction_path_json: str):
        predicted_frames = super()._extract_data_from_json_file(prediction_path_json)
        num_frames = len(predicted_frames)
        num_objects = 0
        for frame in predicted_frames:
            try:
                objects_in_frame = frame["objects"]
                num_objects += len(objects_in_frame)
            except:
                continue # No objects detected
        stats = VideoCompareStats()
        stats.num_frames_failed = 0
        stats.failed_pairs = {}
        stats.num_frames = num_frames
        stats.num_pairs = num_objects
        self._test_case.result.add_info(CaseResultInfo.VIDEO_STATS, stats)

    def _compare_internal(self, gt_path_json: str, prediction_path_json: str):
        fps_value = self._get_achieved_fps()
        fps_target_value = self._get_target_fps()
        fps_vs_target_percent = round((fps_value / fps_target_value) * 100, 1)
        pass_threshold_percent = self._get_pass_fps_threshold_percent()
        fps_pass_threshold = self._get_pass_fps_threshold()

        self._logger.info("Test performance benchmark results:")
        self._logger.info("\t- achieved: {} FPS".format(fps_value))
        self._logger.info("\t- target KPI: {} FPS".format(fps_target_value))
        self._logger.info("\t- pass threshold: {}% of {} FPS KPI = {} FPS".format(pass_threshold_percent, fps_target_value, fps_pass_threshold))

        if fps_value < fps_pass_threshold:
            self._test_case.result.add_error("Achieved {} FPS which is BELOW expectation. Pass threshold: {}% of {} KPI FPS = {} FPS"
                                             .format(fps_value, pass_threshold_percent, fps_target_value, fps_pass_threshold))
        self._save_fps_results()
        self._save_inference_results(prediction_path_json)
