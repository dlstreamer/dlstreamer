# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import logging
import numpy as np

from textwrap import wrap
from video_comparators.utils import ClassProvider, CheckLevel
from utils import GTComparisionError


class RawInferenceMetaComparator(ClassProvider):
    __action_name__ = "raw inference meta"

    def __init__(self, edistance_th: float, check_level: CheckLevel, logger: logging.Logger = None):
        self._logger = logger if logger else logging.getLogger()
        self._dist_thr = edistance_th
        self.num_labels = 0
        self.num_labels_failed = 0

        self._logger.info(
            "Running Raw Inference Meta comparator with following parameters:\n"
            "\t\t Euclidean distance threshold (0 = disabled): {} \n".format(self._dist_thr))


    def compare(self, infer_result: list, reference: list, timestamp: int):
        self._compare_tracker_id(infer_result, reference)

        # TODO just temporary workaround to hide problem with missed layer_name with ME
        # for item in infer_result:
        #     if 'layer_name' not in item:
        #         item['layer_name'] = "empty"
        pr_meta = sorted(
            [item for item in infer_result if 'layer_name' in item], key=lambda i: i['layer_name'])
        gt_meta = sorted(
            [item for item in reference if 'layer_name' in item], key=lambda i: i['layer_name'])

        if pr_meta == gt_meta:
            return

        if len(pr_meta) != len(gt_meta):
            raise GTComparisionError("Additional metas have different size: {}\t///\t{}".format(
                len(pr_meta), len(gt_meta)))

        for pr_meta_item, gt_meta_item in zip(pr_meta, gt_meta):
            if len(pr_meta_item) != len(gt_meta_item):
                raise GTComparisionError("Meta objects have different size: {}\t///\t{}".format(
                    pr_meta_item, gt_meta_item))

            if set(gt_meta_item) != set(pr_meta_item):
                raise GTComparisionError("Meta objects have different content: {}\t///\t{} ".format(
                    pr_meta_item, gt_meta_item))

            self._compare_labels(pr_meta_item, gt_meta_item, timestamp)
            self._compare_raw_tensor(pr_meta_item, gt_meta_item)

    def _compare_tracker_id(self, gt_meta: list, pr_meta: list):
        gt_meta_id = [item["id"] for item in gt_meta if 'id' in item]
        pr_meta_id = [item["id"] for item in pr_meta if 'id' in item]
        if len(gt_meta_id) == len(pr_meta_id) == 1:
            if gt_meta_id[0] != pr_meta_id[0]:
                raise GTComparisionError("Meta object's data has different tracker id:\n{}\t///\t{}".format(
                    pr_meta_id, gt_meta_id))
        elif len(gt_meta_id) != len(pr_meta_id):
            raise GTComparisionError("Meta object's data has different tracker id:\n{}\t///\t{}".format(
                pr_meta_id, gt_meta_id))

    def _compare_labels(self, pr_meta_item, gt_meta_item, timestamp):
        self.num_labels += 1
        if 'name' in pr_meta_item and 'name' in gt_meta_item:
            if pr_meta_item['name'] == gt_meta_item['name']:
                if 'label' in pr_meta_item and 'label' in gt_meta_item:
                    if pr_meta_item['label'] != gt_meta_item['label']:
                        ## The pr_meta_item['name'] == age validation should go through _compare_raw_tensor pass
                        if pr_meta_item['name'] != 'age':
                            self.num_labels_failed += 1
                            self._logger.info("\n \t\t >>> [{}] Meta objects {} have different labels: {} /// {}".format(timestamp,
                                                                                                                         pr_meta_item['name'],
                                                                                                                         pr_meta_item['label'],
                                                                                                                         gt_meta_item['label']))

    def _compare_raw_tensor(self, pr_meta_item: dict, gt_meta_item: dict):
        if 'data' in pr_meta_item and 'data' in gt_meta_item:
            inf_results = pr_meta_item['data']
            reference = gt_meta_item['data']

            if len(set(inf_results)) == 1:
                self._compare_metas_by_single_values(inf_results[0], reference[0])
            else:
                self._check_euclidean_distance(inf_results, reference)

    def _compare_metas_by_single_values(self, inf_results: float, reference: float):
        # If the result and reference differs more than 10% rise error
        percentage_error_exceeds_thr: bool = 10 < (abs(1 - (inf_results / reference)) * 100)
        if percentage_error_exceeds_thr == True:
            raise GTComparisionError(
                "Meta object's data has different values:\n{}\t///\t{}".format(inf_results, reference))

    def _compare_metas_by_values(self, inf_results: list, reference: list):
        if set(inf_results) != set(reference):
                raise GTComparisionError(
                    "Meta object's data has different values:\n{}\t///\t{}".format(inf_results, reference))

    def _check_euclidean_distance(self, inf_results: list, reference: list):
        # Zero means disabled
        if self._dist_thr == 0:
            return

        dist = np.linalg.norm(self._normalize(np.array(inf_results)) - self._normalize(np.array(reference)))
        max_print_len = 15
        if dist > self._dist_thr:
            strs = (', '.join(map(str, arr[:max_print_len])) +
                    ', ...' if len(arr) > max_print_len else '' for arr in (inf_results, reference))
            strs = ['\n'.join(wrap(s)) for s in strs]
            raise GTComparisionError(
                "Euclidean distance between raw tensors is above threshold: {}\n"
                "prediction:[\n{}]\n"
                "reference:[\n{}]".format(dist, strs[0], strs[1]))

    def _normalize(self, arr: np.ndarray, a: float = -1., b: float = 1.) -> np.ndarray:
        return (b - a) * (arr - np.min(arr)) / np.ptp(arr) + a
