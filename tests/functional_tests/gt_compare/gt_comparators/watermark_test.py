# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import json
import os
import logging
import cv2

from gt_comparators.base_gt_comparator import BaseGTComparator
from gt_comparators.base_gt_comparator import RuntimeTestCase
from pipeline_test.regression_test.config_keys import *


def get_histograms(video_path: str):
    video_object = cv2.VideoCapture(video_path)
    histograms = []
    while True:
        ret, frame = video_object.read()
        if not ret:
            break
        histogram_size = 32
        gt_gray_image = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        histogram = cv2.calcHist([gt_gray_image], [0], None, [
                                 histogram_size], [0, 256])
        assert len(histogram) == histogram_size
        histograms.append([item[0] for item in histogram.tolist()])
    return histograms


def compare_hist(hist1: list, hist2: list):
    c = 0
    # Euclidean Distance between gt_histogram and test_histogram
    i = 0
    while i < len(hist1) and i < len(hist2):
        c += (hist1[i] - hist2[i]) ** 2
        i += 1
    c = c ** (1 / 2)
    return c


PRINT_THRESHOLD = 700


class HistogramComparator(BaseGTComparator):
    def __init__(self, test_case: RuntimeTestCase, logger: logging.Logger = None):
        super().__init__(test_case, logger if logger else logging.getLogger())

    def _get_artifacts_paths(self):
        video_file = os.path.join(self._test_case.input[ARTIFACTS_PATH_FIELD], self._test_case.input[OUT_VIDEO])
        prediction_path = os.path.join(
            self._test_case.input[ARTIFACTS_PATH_FIELD], self._test_case.input[GT_FILE_NAME_FIELD])
        return video_file, prediction_path

    def _init_prediction_path(self):
        video_file, prediction_path = self._get_artifacts_paths()
        # to prevent mixing results from different pipelines
        if os.path.exists(video_file):
            self._logger.debug("Removed old video file")
            os.remove(video_file)
        self._pred_path = prediction_path
        self._prepare_prediction_folder()

    def pipeline_results_processing(self):
        video_file, prediction_path = self._get_artifacts_paths()
        test_histograms = get_histograms(video_file)
        with open(prediction_path, "w") as write_file:
            json.dump(test_histograms, write_file)
        return prediction_path

    def _extract_data_from_json_file(self, file_path: str) -> list:
        if not os.path.exists(file_path):
            raise FileExistsError("The {} path to ground truth json file not found".format(file_path))
        with open(file_path, 'r') as gt_file:
            # last line is empty and last symbol of each line is '\n'
            return json.load(gt_file)

    def _compare_internal(self, gt_path_json: str, prediction_path_json: str):
        if not os.path.exists(gt_path_json) or not os.path.exists(prediction_path_json):
            raise FileExistsError(
                "Path to ground truth json file not found\nPossible invalid paths:\n{}\n{}".format(
                    gt_path_json, prediction_path_json))

        test_histograms = self._extract_data_from_json_file(prediction_path_json)
        gt_histograms = self._extract_data_from_json_file(gt_path_json)

        if len(test_histograms) != len(gt_histograms):
            err_msg = "Number of processed frames doesn't match: {} /// {}".format(
                len(test_histograms), len(gt_histograms))
            self._test_case.result.add_error(err_msg)
            return
        diffs = []
        frame_diff_threshold = self._test_case.input.get("dataset.frame_diff_threshold")
        print_threshold = self._test_case.input.get("dataset.print_threshold", PRINT_THRESHOLD)
        for idx in range(len(test_histograms)):
            diffs.append(compare_hist(test_histograms[idx], gt_histograms[idx]))
        for idx, diff in enumerate(diffs):
            if diff > print_threshold:
                self._logger.info("idx: {} diff: {}".format(idx, diff))
            if diff >= frame_diff_threshold:
                self._test_case.result.add_error("idx: {} diff: {} frame_diff_threshold: {}".
                                                 format(idx, diff, frame_diff_threshold))
