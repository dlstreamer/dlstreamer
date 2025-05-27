# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import os
import logging
import glob

LABELS_SUFFIX = ".label"
LABELS_PATH_SUFFIX = ".label.path"


class LabelsExplorer:
    def __init__(self, logger: logging.Logger = None):
        self._logger = logger if logger else logging.getLogger()
        self._labels_dir = os.environ.get("LABELS_PATH")

    def generate_paths_ip(self, test_set: dict):
        key_prefix_list = list()
        for test_set_key in test_set:
            if test_set_key.endswith(LABELS_SUFFIX):
                key_prefix_list.append(
                    test_set_key[0: -len(LABELS_SUFFIX)])

        if key_prefix_list:
            for key_prefix in key_prefix_list:
                if (key_prefix + LABELS_PATH_SUFFIX) in test_set:
                    continue

                labels_name = test_set[key_prefix + LABELS_SUFFIX]

                if labels_name is None:
                    raise ValueError("labels_name is None")

                res = self._get_labels_path(labels_name, key_prefix)
                test_set.update(res)

    def _get_labels_path(self, labels_name, key_prefix):
        if not self._labels_dir or not os.path.isdir(self._labels_dir):
            raise Exception(
                f"Path {self._labels_dir} is not directory. Please specify LABELS_PATH environment variable")
        labels_json = labels_name if os.path.isfile(labels_name) else self._search_labels(labels_name)
        if labels_json:
            self._logger.debug(f"Taken labels: {labels_json}")

        if not labels_json or not os.path.isfile(labels_json):
            raise ValueError(
                f"Can't find labels: {labels_json} in directory: {self._labels_dir}")

        return {key_prefix + LABELS_PATH_SUFFIX: labels_json}

    def _search_labels(self, labels_name):
        labels = None
        grep_files = list(
            glob.glob(self._labels_dir + f'/**/{labels_name}', recursive=True))
        files = [file for file in grep_files]
        if len(files) > 1:
            self._logger.warning(
                f"Found a few labels with name {labels_name}: {files}")
        if files:
            labels = files.pop(0)
        else:
            self._logger.warning(
                f"No labels were found with name {labels_name}")

        return labels
