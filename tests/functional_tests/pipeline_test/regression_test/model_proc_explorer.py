# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import os
import logging
import glob

MODEL_PROC_SUFFIX = ".model_proc"
MODEL_PROC_PATH_SUFFIX = ".model_proc.path"


class ModelProcExplorer:
    def __init__(self, logger: logging.Logger = None):
        self._logger = logger if logger else logging.getLogger()
        self._model_proc_dir_list = os.environ.get("MODEL_PROCS_PATH").split(":")

    def generate_paths_ip(self, test_set: dict):
        key_prefix_list = list()
        for test_set_key in test_set:
            if test_set_key.endswith(MODEL_PROC_SUFFIX):
                key_prefix_list.append(
                    test_set_key[0: -len(MODEL_PROC_SUFFIX)])

        if not key_prefix_list:
            logging.warning("Model-proc is not defined")
            return

        for key_prefix in key_prefix_list:
            if (key_prefix + MODEL_PROC_PATH_SUFFIX) in test_set:
                continue

            model_proc_name = test_set[key_prefix + MODEL_PROC_SUFFIX]

            if model_proc_name is None:
                raise ValueError("model_proc_name is None")

            res = self._get_model_proc_path(model_proc_name, key_prefix)
            test_set.update(res)

    def _get_model_proc_path(self, model_proc_name, key_prefix):
        for model_proc_dir in self._model_proc_dir_list:
            if not model_proc_dir or not os.path.isdir(model_proc_dir):
                continue
            model_proc_json = model_proc_name if os.path.isfile(
                model_proc_name) else self._search_model_proc(model_proc_dir, model_proc_name)
            if model_proc_json:
                self._logger.debug(f"Taken model-proc: {model_proc_json}")
                break

        if not model_proc_json or not os.path.isfile(model_proc_json):
            raise ValueError(
                f"Can't find model-proc: {model_proc_json} : {model_proc_name} using MODEL_PROCS_PATH env variable: {os.environ.get('MODEL_PROCS_PATH')}")

        return {key_prefix + MODEL_PROC_PATH_SUFFIX: model_proc_json}

    def _search_model_proc(self, model_proc_dir: str, model_proc_name: str) -> str:
        model_proc = None
        grep_files = list()
        grep_files = list(
            glob.glob(model_proc_dir + f'/**/{model_proc_name}', recursive=True))
        files = [file for file in grep_files]
        if len(files) > 1:
            self._logger.warning(
                f"Found a few model_procs with name {model_proc_name}: {files}")
        if files:
            model_proc = files.pop(0)
        else:
            self._logger.warning(
                f"No model_procs were found with name {model_proc_name}")

        return model_proc
