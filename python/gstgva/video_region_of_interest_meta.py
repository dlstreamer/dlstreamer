# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

## @file video_region_of_interest_meta.py
#  @brief This file contains VideoRegionOfInterestMeta class which mimics GstVideoRegionOfInterestMeta standard C structure and provides read access 
# to bounding box coordinates (in pixels), roi type, and more.

import ctypes

import gi
gi.require_version('GstVideo', '1.0')
gi.require_version('GLib', '2.0')
gi.require_version('Gst', '1.0')

from gi.repository import GstVideo, GLib, GObject, Gst
from .util import libgst, libgobject, libgstvideo, GLIST_POINTER

## @brief This class mimics GstVideoRegionOfInterestMeta standard C structure and provides read access to bounding box coordinates (in pixels), roi 
# type, and more. To see full fields list, turn to GstVideoRegionOfInterestMeta official documentation, and use this VideoRegionOfInterestMeta object
# as simple structure with fields accessible for reading.
# It's not expected that you will write data into or modify VideoRegionOfInterestMeta. Instead, you should create region_of_interest.RegionOfInterest 
# object with video_frame.VideoFrame.add_region() call
class VideoRegionOfInterestMeta(ctypes.Structure):
    _fields_ = [
        ('_meta_flags', ctypes.c_int),
        ('_info', ctypes.c_void_p),
        ('roi_type', ctypes.c_int),
        ('id', ctypes.c_int),
        ('parent_id', ctypes.c_int),
        ('x', ctypes.c_int),
        ('y', ctypes.c_int),
        ('w', ctypes.c_int),
        ('h', ctypes.c_int),
        ('_params', GLIST_POINTER)
    ]

    ## @brief Get VideoRegionOfInterestMeta.roi_type as a string (type/class of detected objects, such as "vehicle", "person", etc)
    # @return VideoRegionOfInterestMeta.roi_type as a string
    def get_roi_type(self) -> str:
        return GLib.quark_to_string(self.roi_type)

    ## @brief Set VideoRegionOfInterestMeta.roi_type from string (type/class of detected objects, such as "vehicle", "person", etc)
    # @param new_type string value to set to VideoRegionOfInterestMeta.roi_type
    def set_roi_type(self, new_type: str) -> None:
        self.roi_type = GLib.quark_from_string(new_type)

    ## @brief Iterate by VideoRegionOfInterestMeta instances attached to buffer
    # @param buffer buffer with GstVideoRegionOfInterestMeta instances attached
    # @return generator for VideoRegionOfInterestMeta instances attached to buffer
    @classmethod
    def iterate(cls, buffer: Gst.Buffer):
        try:
            meta_api = hash(GObject.GType.from_name(
                "GstVideoRegionOfInterestMetaAPI"))
        except:
            return
        gpointer = ctypes.c_void_p()
        while True:
            try:
                value = libgst.gst_buffer_iterate_meta_filtered(hash(buffer),
                                                                ctypes.byref(
                                                                    gpointer),
                                                                meta_api)
            except:
                value = None

            if not value:
                return

            yield ctypes.cast(value, ctypes.POINTER(VideoRegionOfInterestMeta)).contents


VIDEO_REGION_OF_INTEREST_POINTER = ctypes.POINTER(VideoRegionOfInterestMeta)
libgstvideo.gst_video_region_of_interest_meta_get_param.argtypes = [VIDEO_REGION_OF_INTEREST_POINTER,
                                                                    ctypes.c_char_p]
libgstvideo.gst_video_region_of_interest_meta_get_param.restype = ctypes.c_void_p

libgstvideo.gst_video_region_of_interest_meta_add_param.argtypes = [VIDEO_REGION_OF_INTEREST_POINTER,
                                                                    ctypes.c_void_p]
libgstvideo.gst_video_region_of_interest_meta_add_param.restype = None
