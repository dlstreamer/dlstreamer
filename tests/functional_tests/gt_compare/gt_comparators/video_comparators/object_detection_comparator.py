# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import logging

from dataclasses import dataclass, field
from collections import namedtuple
from video_comparators.utils import ClassProvider, BBox
from utils import GTComparisionError
from tabulate import tabulate
from typing import List
import numpy as np


@dataclass
class ObjectDetComparatorConfig:
    prob_thr: float
    abs_thr: float
    rel_thr: float
    iou_thr: float
    error_thr: float
    low_conf_thr: float


@dataclass
class BBoxPair:
    gt_prob: float = 0
    pred_prob: float = 0
    iou: float = 0
    iou_ok: bool = True
    prob_ok: bool = True
    timestamp: int = -1


def make_table(*args, **kwargs):
    """Wrapper function for `tabulate` to unify table styles across tests."""
    tablefmt = kwargs.pop('tablefmt', 'orgtbl')
    table = tabulate(*args, tablefmt=tablefmt, **kwargs)
    return table


def make_pairs_table(pairs: List[BBoxPair]):
    if pairs:
        table_rows = [pair.__dict__.values() for pair in pairs]
        table_keys = pairs[0].__dict__.keys()
        error_table = '\n' + make_table(table_rows, table_keys) + '\n'
        return error_table


@dataclass
class BBoxCompareStats:
    num_total_pairs: int = 0
    num_frames_failed: int = 0
    num_frames: int = 0
    failed_pairs: List[BBox] = field(default_factory=list)

    def update(self, new_failed_pairs: List[BBox], new_num_pairs: int):
        self.num_frames += 1
        self.num_total_pairs += new_num_pairs
        if new_failed_pairs:
            self.num_frames_failed += 1
            self.failed_pairs += new_failed_pairs

    def get_message(self, rows_limit: int = 0) -> str:
        percent_frames = 100. * self.num_frames_failed / self.num_frames
        percent_pairs = 100. * len(self.failed_pairs) / self.num_total_pairs
        message = "\nframes failed/compared {}/{} ({:.2f}%)".format(self.num_frames_failed,
                                                                    self.num_frames, percent_frames)
        message += "\nbbox pairs failed/compared {}/{} ({:.2f}%)".format(len(self.failed_pairs),
                                                                         self.num_total_pairs, percent_pairs)
        if self.failed_pairs:
            message += make_pairs_table(self.failed_pairs if not rows_limit else self.failed_pairs[:rows_limit])
            message += "...<truncated>" if len(self.failed_pairs) > rows_limit else ""
        return message


class BBoxesComparisionError(Exception):
    """Raised when a comparison between a boxes has failed"""


def _intersection_over_union(pred_bbox: BBox, ref_bbox: BBox):
    """
    :return: float value of IOU metric for one pair of bound boxes
    """
    if (pred_bbox.xmax < ref_bbox.xmin) or (
            ref_bbox.xmax < pred_bbox.xmin) or (
            ref_bbox.ymax < pred_bbox.ymin) or (pred_bbox.ymax < ref_bbox.ymin):
        return 0
    intersection_coord = BBox()
    intersection_coord.xmin = max(pred_bbox.xmin, ref_bbox.xmin)
    intersection_coord.xmax = min(pred_bbox.xmax, ref_bbox.xmax)
    intersection_coord.ymin = max(pred_bbox.ymin, ref_bbox.ymin)
    intersection_coord.ymax = min(pred_bbox.ymax, ref_bbox.ymax)
    intersection_width = intersection_coord.xmax - intersection_coord.xmin
    intersection_height = intersection_coord.ymax - intersection_coord.ymin
    intersection_square = intersection_width * intersection_height

    bbox_1_area = (pred_bbox.xmax - pred_bbox.xmin) * (pred_bbox.ymax - pred_bbox.ymin)
    bbox_2_area = (ref_bbox.xmax - ref_bbox.xmin) * (ref_bbox.ymax - ref_bbox.ymin)

    union_square = bbox_1_area + bbox_2_area - intersection_square
    if union_square == 0:
        raise RuntimeError("Division by zero. The area of the union is zero")

    iou = intersection_square / union_square
    return iou if not np.isnan(iou) else 0


class ObjectDetectionComparator(ClassProvider):
    __action_name__ = "object_detection"

    def __init__(self, config: ObjectDetComparatorConfig, logger: logging.Logger = None):
        self._logger = logger if logger else logging.getLogger()
        self._config = config
        self.statistics = BBoxCompareStats()

        self._logger.debug(
            "Running Object Detection comparator with following parameters:\n"
            "\t\t Probability threshold: {} \n"
            "\t\t Absolute difference threshold: {}\n"
            "\t\t Relative difference threshold: {}\n"
            "\t\t IOU threshold: {}\n"
            "\t\t Error threshold: {}\n"
            "\t\t Low confidence threshold: {}".format(self._config.prob_thr, self._config.abs_thr, self._config.rel_thr,
                                                       self._config.iou_thr, self._config.error_thr, self._config.low_conf_thr))

    def reset_stats(self):
        self.statistics = BBoxCompareStats()

    def compare(self, reference: List[BBox], infer_result: List[BBox], timestamp):
        pairs = self._get_pairs(reference, infer_result)
        failed_res_pairs_detection = list(filter(lambda pair: not pair.prob_ok or not pair.iou_ok, pairs))
        for pair in failed_res_pairs_detection:
            pair.timestamp = timestamp
        self.statistics.update(failed_res_pairs_detection, len(pairs))
        if failed_res_pairs_detection:
            raise BBoxesComparisionError()

    def _filter_bboxes_by_threshold(self, bboxes: List[BBox]) -> List[BBox]:
        return list(filter(lambda bbox: bbox.prob >= self._config.prob_thr, bboxes))

    def _create_pair(self, gt_bbox: BBox, pred_bbox: BBox, iou) -> BBoxPair:
        bbox_pair = BBoxPair()
        bbox_pair.gt_prob = gt_bbox.prob
        bbox_pair.pred_prob = pred_bbox.prob
        bbox_pair.iou = iou

        # pass all checks for pair of missed bbox and bbox with low confidence
        pass_checks = (gt_bbox.is_invalid() and (pred_bbox.prob < self._config.low_conf_thr)) or (
            pred_bbox.is_invalid() and (gt_bbox.prob < self._config.low_conf_thr))
        if pass_checks:
            return bbox_pair

        bbox_pair.iou_ok = bbox_pair.iou > self._config.iou_thr

        if (pred_bbox.prob == -1 and gt_bbox.prob != -1) or (pred_bbox.prob != -1 and gt_bbox.prob == -1):
            bbox_pair.prob_ok = False
            return bbox_pair

        abs_diff = abs(pred_bbox.prob - gt_bbox.prob)
        relative_diff = abs(pred_bbox.prob - gt_bbox.prob) / max(pred_bbox.prob, gt_bbox.prob)
        bbox_pair.prob_ok = abs_diff < self._config.abs_thr or relative_diff < self._config.rel_thr

        return bbox_pair

    """
    matrix with IOU values is constructed for every class in every batch
    (rows -- reference bound boxes, columns -- predicted bound boxes)
    pairs of bound boxes from reference and prediction sets are chosen by taking
    the maximum value from this matrix until all possible ones are found
    :param prediction: filtered prediction data
    :param reference: filtered reference data
    :return: overall status
    """

    def _find_matches(self, pred_detections: List[BBox], gt_detections: List[BBox]):
        pairs = []
        gt_detections_remaining = gt_detections.copy()
        pred_detections_remaining = pred_detections.copy()

        ref_detect_labels = set([bbox.label for bbox in gt_detections])
        ref_detect_labels = ref_detect_labels.union(set([bbox.label for bbox in pred_detections]))
        matrix = {}
        for label in ref_detect_labels:
            matrix[label] = np.zeros((len(gt_detections) + 1, len(pred_detections) + 1))
        for i, gt_bbox in enumerate(gt_detections):
            for j, pred_bbox in enumerate(pred_detections):
                if pred_bbox.label == gt_bbox.label:
                    matrix[pred_bbox.label][i][j] = _intersection_over_union(pred_bbox, gt_bbox)

        for label in ref_detect_labels:
            ref_len = sum(bbox.label == label for bbox in gt_detections)
            pred_len = sum(bbox.label == label for bbox in pred_detections)
            expected_pairs_count = min(pred_len, ref_len)

            for _ in range(expected_pairs_count):
                # There are no boxes with IOU more than zero left for specified label.
                # So no more more pairs can be created
                if np.count_nonzero(matrix[label]) == 0:
                    break

                # Search pair of detected objects with max IoU
                i, j = np.unravel_index(
                    np.argmax(matrix[label], axis=None), matrix[label].shape)
                gt_bbox = gt_detections[i]
                pred_bbox = pred_detections[j]
                pairs.append(self._create_pair(gt_bbox, pred_bbox, matrix[label][i][j]))
                gt_detections_remaining.remove(gt_bbox)
                pred_detections_remaining.remove(pred_bbox)
                # Fill matrix with zeroes for found objects
                matrix[label][i] = np.zeros(matrix[label].shape[1])
                matrix[label][:, j] = np.zeros(matrix[label].shape[0])


        # Create dummy pair for remaining boxes. They have zero IOU
        while gt_detections_remaining:
            pairs.append(self._create_pair(gt_detections_remaining.pop(), BBox(), 0.0))

        while pred_detections_remaining:
            pairs.append(self._create_pair(BBox(), pred_detections_remaining.pop(), 0.0))

        return pairs

    def _get_pairs(self, reference: List[BBox], infer_result: List[BBox]) -> List[BBoxPair]:
        infer_result_filtered = self._filter_bboxes_by_threshold(infer_result)
        reference_filtered = self._filter_bboxes_by_threshold(reference)
        return self._find_matches(infer_result_filtered, reference_filtered)