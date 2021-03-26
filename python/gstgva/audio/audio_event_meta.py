# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

import ctypes
from ..util import GLIST_POINTER

# AudioEventMeta
class AudioEventMeta(ctypes.Structure):
    _fields_ = [
        ('_meta_flags', ctypes.c_int),
        ('_info', ctypes.c_void_p),
        ('event_type', ctypes.c_int),
        ('id', ctypes.c_int),
        ('start_timestamp', ctypes.c_ulong),
        ('end_timestamp', ctypes.c_ulong),
        ('_params', GLIST_POINTER)
    ]
