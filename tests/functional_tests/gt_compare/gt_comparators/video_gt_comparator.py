# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import logging

import os
import json
from dataclasses import dataclass, field
from base_gt_comparator import BaseGTComparator
from base_gt_comparator import RuntimeTestCase, CaseResultInfo
from video_comparators.object_detection_comparator import ObjectDetectionComparator, ObjectDetComparatorConfig, BBoxesComparisionError
from video_comparators.object_classification_comparator import ObjectClassificationComparator
from video_comparators.raw_inference_meta_comparator import RawInferenceMetaComparator
from utils import GTComparisionError, InconsistentDataError
from collections import namedtuple, OrderedDict
import numpy as np
from video_comparators.utils import CheckLevel, CheckInfoStorage, fails_percent, BBox
from typing import List
from tabulate import tabulate
from itertools import zip_longest
import re

VGTComparatorConfig = namedtuple(
    'VGTComparatorConfig', "odc_config check_mode")


def _filtered_meta_tensors(tensors):
    # returns dictionary of name/label ids(or labels) from tensors
    filtered_tensors = []
    for tensor in tensors:
        # compare label_ids if possible
        if "label_id" in tensor:
            filtered_tensors.append([tensor["name"], tensor["label_id"]])
        elif "label" in tensor:
            filtered_tensors.append([tensor["name"], tensor["label"]])
    return filtered_tensors

@dataclass
class VideoCompareError:
    timestamp: int = -1
    error: str = None

@dataclass
class VideoCompareStats:
    num_frames: int = 0
    num_frames_failed: int = 0
    num_pairs: int = 0
    failed_pairs: List[BBox] = field(default_factory=list)

    # Lists of missing frames timestamps - means frame is missing in predicted/gt
    missing_frames_predicted: List[int] = field(default_factory=list)
    missing_frames_gt: List[int] = field(default_factory=list)

    cmp_errors: List[VideoCompareError] = field(default_factory=list)

    def add_error(self, timestamp: int, error: str):
        vce = VideoCompareError()
        vce.timestamp = timestamp
        vce.error = error
        self.cmp_errors.append(vce)

    def get_errors_table(self, rows_limit: int = 0):
        if not self.cmp_errors:
            return
        rows = [item.__dict__.values() for item in self.cmp_errors]
        tail = ""
        if rows_limit and len(rows) > rows_limit:
            rows = rows[:rows_limit]
            tail = "\n ...<truncated>"
        cols = self.cmp_errors[0].__dict__.keys()
        return '\n' + tabulate(rows, headers=cols, tablefmt='orgtbl') + tail

class VideoGTComparator(BaseGTComparator):
    def __init__(self, test_case: RuntimeTestCase, config: VGTComparatorConfig, edistance_th: float, lbl_max_err_thr: float, logger: logging.Logger = None):
        _logger = logger if logger else logging.getLogger()
        super().__init__(test_case, _logger)

        self._mode = config.check_mode
        self._error_thr = config.odc_config.error_thr
        self.object_detection_comparator = ObjectDetectionComparator(config.odc_config, _logger)
        self._oc_comparator = ObjectClassificationComparator(config.check_mode, logger=_logger)
        self._rim_comparator = RawInferenceMetaComparator(edistance_th, config.check_mode, logger=_logger)
        self._lbl_max_err_thr = lbl_max_err_thr
        self.test_name = test_case.name

    def _equal_classes(self, target, result):
        for target_obj, result_obj in zip(target, result):
            if 'class' not in target_obj:
                continue
            # 'class' contains key-value dictionary, e.g. 'class': { 'age' : 23 }
            if not self._oc_comparator.compare(target_obj['class'], result_obj['class']):
                return False
        return True

    def _filtered_meta_objects(self, meta_objects) -> List[BBox]:
        filtered_meta_objects = list()
        for meta_object in meta_objects:
            bbox_essence = BBox()
            for meta_object_name, meta_object_value in meta_object.items():
                if meta_object_name == "detection":
                    bbox_essence.xmin = meta_object_value.get("bounding_box", {}).get("x_min", 0)
                    bbox_essence.ymin = meta_object_value.get("bounding_box", {}).get("y_min", 0)
                    bbox_essence.xmax = meta_object_value.get("bounding_box", {}).get("x_max", 0)
                    bbox_essence.ymax = meta_object_value.get("bounding_box", {}).get("y_max", 0)
                    bbox_essence.prob = meta_object_value.get("confidence", -1)
                    label_id = meta_object_value.get("label_id")
                    if label_id is None:
                        self._logger.debug("The key 'label_id' wasn't found in output meta object. "
                                        "Will try to get class from 'label'")
                        try:
                            label = meta_object_value.get("label", "")
                            bbox_essence.classes.append(["label", int(float(label))])
                        except ValueError:
                            bbox_essence.classes.append(["label", label])  # unable to convert to float. Will compare strings then
                    else:
                        bbox_essence.classes.append(["label", label_id])
                elif meta_object_name == "tensors":
                    bbox_essence.additional_meta += meta_object_value
                elif meta_object_name == "id":
                    # TODO: check if it's still in use
                    bbox_essence.additional_meta.append({"id": int(meta_object_value)})
            if 'tensors' in meta_object:
                bbox_essence.classes += _filtered_meta_tensors(meta_object['tensors'])
            bbox_essence.init_label()
            filtered_meta_objects.append(bbox_essence)
        return filtered_meta_objects

    def _extract_data_from_json_file(self, file_path: str) -> dict:
        if not os.path.exists(file_path):
            raise FileExistsError("The {} path to json file not found".format(file_path))
        with open(file_path, 'r') as file:
            raw_frames = []
            for line in file:
                line = line.strip()  # Remove leading/trailing spaces & newlines
                if not line:
                    continue  # Skip empty lines
                if line.startswith("[") and line.endswith("]"):
                    line = line[1:-1]  # Remove brackets
                if line:  # Ensure it's still not empty before parsing
                    try:
                        raw_frames.append(json.loads(line))
                    except json.JSONDecodeError as e:
                        print(f"Skipping invalid JSON line: {line} | Error: {e}")
        if len(raw_frames) == 0 :
            raise FileExistsError("The file {} is empty".format(file_path))
        timestamp_to_objects = dict()
        last_ts = -1
        # check if this is a multi stream test case
        multi_stream_substr = re.search("multi_stream", file_path)
        if multi_stream_substr:
            self._logger.info("\tMulti stream case {}. Skip timestamp verification since output is a concatination of many sub-output files.".format(file_path))
        for frame_idx, frame in enumerate(raw_frames):
            timestamp = frame["timestamp"]
            objects = frame["objects"]
            if last_ts > timestamp and not multi_stream_substr:
                raise InconsistentDataError(
                    "timestamps should be ascending idx: {} prev: {} timestamp: {}".format(frame_idx, last_ts, timestamp))
            timestamp_to_objects[timestamp] = self._filtered_meta_objects(objects)
            last_ts = timestamp
        return timestamp_to_objects

    def _compare_internal(self, gt_path: str, results_path: str):
        self.object_detection_comparator.reset_stats()
        self._logger.info("gt: {} result: {}".format(gt_path, results_path))
        predicted_frames = self._extract_data_from_json_file(results_path)
        gt_frames = self._extract_data_from_json_file(gt_path)

        stats = VideoCompareStats()
        self._rim_comparator.num_labels = 0
        self._rim_comparator.num_labels_failed = 0

        for timestamp in gt_frames.keys():
            if timestamp not in predicted_frames:
                stats.missing_frames_predicted.append(timestamp)
                self._logger.info("missed frame in predicted timestamp: {}".format(timestamp))
                predicted_frames[timestamp] = []
        for timestamp in predicted_frames.keys():
            if timestamp not in gt_frames:
                stats.missing_frames_gt.append(timestamp)
                self._logger.info("missed frame in gt timestamp: {}".format(timestamp))
                gt_frames[timestamp] = []

        meta_checks_storage = CheckInfoStorage(self._error_thr)
        for timestamp, gt_objects in gt_frames.items():
            predicted_objects = predicted_frames[timestamp]
            try:
                stats.num_frames += 1
                self.object_detection_comparator.compare(gt_objects, predicted_objects, timestamp)
                # check detection classes set is the same for test & target
                self.compare_classification_results(gt_objects, predicted_objects, meta_checks_storage)
                if self._mode == CheckLevel.full:
                    self.compare_additional_infer_meta(gt_objects, predicted_objects, timestamp)

            except GTComparisionError as err:
                stats.add_error(timestamp, str(err))
                stats.num_frames_failed += 1
            except BBoxesComparisionError as err:
                stats.num_frames_failed += 1
                # failed pairs are collected by object_detection_comparator.statistics

        # Check the meta labels failures value
        if self._rim_comparator.num_labels > 0:
            lbl_err_thr = self._rim_comparator.num_labels_failed / self._rim_comparator.num_labels
            if lbl_err_thr > self._lbl_max_err_thr:
                _err = "Meta object's label data failures exceed allowable threashold (lbl_err_thr: {:.4f} lbl_max_err_thr: {:.4f})".format(lbl_err_thr, self._lbl_max_err_thr)
                stats.add_error(0, str(_err))
            self._logger.info("labels total: {}, failed: {}, lbl_err_thr: {:.4f}, lbl_max_err_thr: {:.4f}".format(self._rim_comparator.num_labels,
                                                                                                                  self._rim_comparator.num_labels_failed,
                                                                                                                  lbl_err_thr,
                                                                                                                  self._lbl_max_err_thr))
        # Copy failed pairs
        stats.num_pairs = self.object_detection_comparator.statistics.num_total_pairs
        stats.failed_pairs = self.object_detection_comparator.statistics.failed_pairs

        self._test_case.result.add_info(CaseResultInfo.VIDEO_STATS, stats)
        max_printed_error = 35
        if self.object_detection_comparator.statistics.failed_pairs:
            self._test_case.result.add_error(self.object_detection_comparator.statistics.get_message(max_printed_error))
        if stats.cmp_errors:
            self._test_case.result.add_error(stats.get_errors_table(max_printed_error))
        self._logger.info("number of tested frames: {}, objects: {}".format(
            stats.num_frames, stats.num_pairs))
        if self._mode == CheckLevel.soft and not meta_checks_storage.is_passed():
            self._logger.error("Meta checks/fails: {}/{}".format(meta_checks_storage.checks,
                                                                 meta_checks_storage.fails))
            raise GTComparisionError(
                "Results differs from groundruth by more than {}%, (meta checks fails: {}%)".format(
                    self._error_thr *
                    100, fails_percent(meta_checks_storage)))

    def compare_classification_results(self, reference: List[BBox], infer_result: List[BBox], meta_check_storage: CheckInfoStorage):
        for target_obj, result_obj in zip(reference, infer_result):
            if len(target_obj.classes) == 0:
                continue
            # 'class' contains key-value dictionary, e.g. 'class': { 'age' : 23 }
            self._oc_comparator.compare(target_obj.classes, result_obj.classes, self._mode, meta_check_storage)

    def compare_additional_infer_meta(self, gt_objects: List[BBox], pred_objects: List[BBox], timestamp: int):
        for gt_obj, pred_obj in zip_longest(gt_objects, pred_objects):
            self._rim_comparator.compare(pred_obj.additional_meta if pred_obj else [], gt_obj.additional_meta if gt_obj else [], timestamp)


def create_video_gt_comparator(test_case, args, logger) -> VideoGTComparator:
    vgt_comparator_config = VGTComparatorConfig(
        ObjectDetComparatorConfig(
            args.p_thr, args.a_thr, args.r_thr, args.iou_thr, args.error_thr, args.low_conf_thr), args.check_level)
    return VideoGTComparator(test_case, vgt_comparator_config, args.edistance_thr, args.lbl_max_err_thr, logger)