# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

import ctypes
from gi.repository import GObject

from .util import libgst, libgstva

GVA_TENSOR_MAX_RANK = 8


class GVATensorMeta(ctypes.Structure):
    _fields_ = [
        ('_meta_flags', ctypes.c_int),
        ('_info', ctypes.c_void_p),
        ('data', ctypes.c_void_p)
        ]

    @classmethod
    def add_tensor_meta(cls, buffer):
        try:
            value = libgst.gst_buffer_add_meta(hash(buffer),
                                               libgstva.gst_gva_tensor_meta_get_info(),
                                               None)
        except Exception as error:
            value = None

        if not value:
            return

        return ctypes.cast(value, GST_GVA_TENSOR_META_POINTER).contents

    @classmethod
    def iterate(cls, buffer):
        try:
            meta_api = hash(GObject.GType.from_name("GstGVATensorMetaAPI"))
        except:
            return

        gpointer = ctypes.c_void_p()
        while True:
            try:
                value = libgst.gst_buffer_iterate_meta_filtered(hash(buffer),
                                                                ctypes.byref(
                                                                    gpointer),
                                                                meta_api)
            except Exception as error:
                value = None

            if not value:
                return

            yield ctypes.cast(value, GST_GVA_TENSOR_META_POINTER).contents


GST_GVA_TENSOR_META_POINTER = ctypes.POINTER(GVATensorMeta)

libgstva.gst_gva_tensor_meta_get_info.argtypes = None
libgstva.gst_gva_tensor_meta_get_info.restype = ctypes.c_void_p
