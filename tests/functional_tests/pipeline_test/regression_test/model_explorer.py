# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import os
import logging
import glob
import xml.etree.ElementTree as ET
from enum import Enum

MODEL_NAME_SUFFIX = '.model.name'
MODEL_TYPE_SUFFIX = '.model.type'
MODEL_PRECISION_SUFFIX = '.model.precision'
MODEL_DIRECTORY_SUFFIX = '.model.directory'
MODEL_PATH_SUFFIX = '.model.path'
MODEL_BIN_PATH_SUFFIX = '.model.bin_path'

PRECISIONS = ["FP32", "FP16", "FP32-INT8", "FP16-INT8", "INT8", "INT4", "INT2"]


class ModelType(Enum):
    IR = "ir"
    ONNX = "onnx"
    TORCHVISION = "torchvision"

    @staticmethod
    def get_type(str_value):
        if str_value == "ir":
            return ModelType.IR
        if str_value == "onnx":
            return ModelType.ONNX
        if str_value == "torchvision":
            return ModelType.TORCHVISION

        return None


class ModelExplorer:
    def __init__(self, logger: logging.Logger = None):
        self._logger = logger if logger else logging.getLogger()
        self._model_directories = self._get_model_dirs_from_env()
        self.missed_models = set()

    def _get_model_dirs_from_env(self):
        cv_sdk_dir = os.environ.get(
            'OPENVINO_DIR', '/opt/intel/openvino/')
        default_models_dirs = os.environ.get('MODELS_PATH', os.path.join(
            cv_sdk_dir, 'deployment_tools', 'intel_models')).split(':')
        return list(filter(lambda p: p, default_models_dirs))

    def generate_paths_ip(self, test_set: dict):
        key_prefix_list = list()
        for test_set_key in test_set:
            if test_set_key.endswith(MODEL_NAME_SUFFIX):
                key_prefix_list.append(
                    test_set_key[0: -len(MODEL_NAME_SUFFIX)])

        if not key_prefix_list:
            raise ValueError("Model name is not defined")

        for key_prefix in key_prefix_list:
            if key_prefix + MODEL_PATH_SUFFIX in test_set:
                continue

            model_name = test_set[key_prefix + MODEL_NAME_SUFFIX]
            model_type = ModelType.get_type(
                test_set.get(key_prefix + MODEL_TYPE_SUFFIX, "ir"))

            if model_type is None or not model_type in ModelType:
                raise ValueError("Unsupported model.type")

            model_precision = test_set.get(
                key_prefix + MODEL_PRECISION_SUFFIX, 'FP32')

            if model_name is None:
                raise ValueError("model_name is None")

            if model_type == ModelType.ONNX:
                res = self._get_onnx_models_path(model_name,
                                                 model_precision, key_prefix)
            elif model_type == ModelType.TORCHVISION:
                res = {} # models from torchvision can be loaded without path
            else:
                res = self._get_ir_models_path(
                    model_name, model_precision, key_prefix)
            test_set.update(res)

    def _get_ir_models_path(self, model_name, model_precision, key_prefix):
        model_xml = None
        model_bin = None

        for model_directory in self._model_directories:
            if not os.path.exists(model_directory) or not os.path.isdir(model_directory):
                self._logger.error(
                    "Path {} does not exist or not directory. Please specify MODELS_PATH environment variable".format(
                        model_directory))
                raise Exception(
                    "Path {} does not exist or not directory. Please specify MODELS_PATH environment variable".format(
                        model_directory))
            model_xml, model_bin = self._serach_model_in_directory(
                model_name, model_precision, model_directory, ModelType.IR)
            if model_xml and model_bin:
                self._logger.debug(
                    "Taken model: {}, {}".format(model_xml, model_bin))
                break

        if not model_xml or not os.path.exists(model_bin):
            raise ValueError("Can't find model: {} with precision: {} in directory: {}".format(
                model_name, model_precision, model_directory))
        if not model_bin or not os.path.exists(model_bin):
            raise ValueError("Can't find bin file fo model: {} with precision: {} in directory: {}".format(
                model_name, model_precision, model_directory))

        return {
            key_prefix + MODEL_PATH_SUFFIX: model_xml,
            key_prefix + MODEL_BIN_PATH_SUFFIX: model_bin
        }

    def _get_onnx_models_path(self, model_name, model_precision, key_prefix):
        model_onnx = None

        for model_directory in self._model_directories:
            if not os.path.exists(model_directory) or not os.path.isdir(model_directory):
                self._logger.error(
                    "Path {} does not exist or not directory".format(model_directory))
                raise Exception(
                    "Path {} does not exist or not directory".format(model_directory))
            model_onnx, _ = self._serach_model_in_directory(
                model_name, model_precision, model_directory, ModelType.ONNX)
            if model_onnx:
                self._logger.debug("Taken model: {}".format(model_onnx))
                break

        if not model_onnx:
            raise ValueError("Can't find model: {} with precision: {} in directory: {}".format(
                model_name, model_precision, model_directory))

        return {
            key_prefix + MODEL_PATH_SUFFIX: model_onnx
        }

    def _serach_model_in_directory(self, model_name, model_precision, model_directory, model_type):
        model = None
        model_bin = None
        model_extension = ".onnx" if model_type == ModelType.ONNX else ".xml"

        grep_files = list(glob.glob(model_directory +
                                    f'/**/{model_name}{model_extension}', recursive=True))

        def doesContainPrecision(file, pr): return (
            pr.lower() in file.lower())

        def doesContainAnyPrecision(file, prs):
            for pr in prs:
                if doesContainPrecision(file, pr):
                    return True
            return False

        model_precision_in_path = '/' + model_precision + '/'

        files = [file for file in grep_files if doesContainPrecision(
            file, model_precision_in_path)]

        if len(files) > 1:
            self._logger.warning(
                "Find a few models with name {}: {}".format(model_name, files))
        if files:
            model = files.pop(0)
        else:
            self._logger.warning(
                "No models were found in the new format of their path: {}".format(model_name))

            model_suffix_precision = '-' + model_precision + model_extension
            suffix_precisions = [('-' + pr + model_extension)
                                 for pr in PRECISIONS if (pr.upper() != "FP32")]
            files = [file for file in grep_files if doesContainPrecision(file, model_suffix_precision) or (
                model_precision.upper() == "FP32" and not doesContainAnyPrecision(
                    file, suffix_precisions)
            )]

            if len(files) > 1:
                self._logger.warning(
                    "Find a few models with name {}: {}".format(model_name, files))
            if files:
                model = files.pop(0)

        if model_type == ModelType.IR and model != None:
            model_bin = os.path.splitext(model)[0] + '.bin'

        if not model:
            self.missed_models.add(model_name)

        return model, model_bin
