# ==============================================================================
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

import sys
import os
import ctypes

MODULE_DIR_PATH = os.path.dirname(__file__)
sys.path.insert(0, MODULE_DIR_PATH)


def register_metadata():
    libgstva = ctypes.CDLL("libdlstreamer_gst.so")
    libgstva.gst_gva_json_meta_get_info.argtypes = None
    libgstva.gst_gva_json_meta_get_info.restype = ctypes.c_void_p
    libgstva.gst_gva_tensor_meta_get_info.argtypes = None
    libgstva.gst_gva_tensor_meta_get_info.restype = ctypes.c_void_p
    libgstva.gst_gva_json_meta_get_info()
    libgstva.gst_gva_tensor_meta_get_info()
