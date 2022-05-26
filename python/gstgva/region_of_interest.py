# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

## @file region_of_interest.py
#  @brief This file contains gstgva.region_of_interest.RegionOfInterest class to control region of interest for particular gstgva.video_frame.VideoFrame with gstgva.tensor.Tensor instances attached

import ctypes
import numpy
from typing import List
from collections import namedtuple

from .tensor import Tensor
from .util import VideoRegionOfInterestMeta
from .util import libgst, libgobject, libgstvideo, GLIST_POINTER

import gi
gi.require_version('GstVideo', '1.0')
gi.require_version('GLib', '2.0')
gi.require_version('Gst', '1.0')
from gi.repository import GstVideo, GLib, GObject, Gst

Rect = namedtuple("Rect", "x y w h")

## @brief This class represents region of interest - object describing detection result (bounding box) and containing
# multiple Tensor objects (inference results) attached by multiple models. For example, it can be region of interest with detected face and recognized age
# and sex of a person. It can be produced by a pipeline with gvadetect with detection model and two gvaclassify
# elements with two classification models. Such RegionOfInterest will have bounding box coordinates filled and will have 3 Tensor objects
# attached - 1 Tensor object with detection result and 2 Tensor objects with classification results coming from 2 classifications
class RegionOfInterest(object):
    ## @brief Get bounding box of the RegionOfInterest as pixel coordinates in original image
    #  @return Bounding box coordinates of the RegionOfInterest
    def rect(self):
        return Rect(x = self.__roi_meta.x,
                    y = self.__roi_meta.y,
                    w = self.__roi_meta.w,
                    h = self.__roi_meta.h)

    ## @brief Get bounding box of the RegionOfInterest as normalized coordinates in the range [0, 1]
    #  @return Bounding box coordinates of the RegionOfInterest
    def normalized_rect(self):
        detection = self.detection()
        return Rect(x = detection['x_min'],
                    y = detection['y_min'],
                    w = detection['x_max'] - detection['x_min'],
                    h = detection['y_max'] - detection['y_min'])

    ## @brief Get class label of this RegionOfInterest
    #  @return Class label of this RegionOfInterest
    def label(self) -> str:
        return GLib.quark_to_string(self.__roi_meta.roi_type)

    ## @brief Get confidence from detection Tensor, last added to this RegionOfInterest
    # @return last added detection Tensor confidence if exists, otherwise None
    def confidence(self) -> float:
        detection = self.detection()
        return detection.confidence() if detection else None

    ## @brief Get object id
    # @return object id as an int, None if failed to get
    def object_id(self) -> int:
        param = self.meta()._params
        object_id_tensor = None
        while param:
            tensor_structure = param.contents.data
            if libgst.gst_structure_has_name(tensor_structure, "object_id".encode('utf-8')):
                object_id_tensor = Tensor(tensor_structure)
                break
            param = param.contents.next
        if object_id_tensor == None:
            return None
        return object_id_tensor['id']

    ## @brief Get all Tensor instances added to this RegionOfInterest
    # @return list of Tensor instances added to this RegionOfInterest
    def tensors(self):
        param = self.meta()._params
        while param:
            tensor_structure = param.contents.data
            # "object_id" is used to store ROI id for tracking
            if not libgst.gst_structure_has_name(tensor_structure, "object_id".encode('utf-8')):
                yield Tensor(tensor_structure)
            param = param.contents.next

    ## @brief Returns detection Tensor, last added to this RegionOfInterest. As any other Tensor, returned detection
    # Tensor can contain arbitrary information. If you use RegionOfInterest based on VideoRegionOfInterestMeta
    # attached by gvadetect by default, then this Tensor will contain "label_id", "confidence", "x_min", "x_max",
    # "y_min", "y_max" fields.
    # If RegionOfInterest doesn't have detection Tensor, it will be created in-place
    # @return detection Tensor, empty if there were no detection Tensor objects added to this RegionOfInterest when
    # this method was called
    def detection(self) -> Tensor:
        for tensor in self.tensors():
            if tensor.is_detection():
                return tensor
        return self.add_tensor('detection')

    ## @brief Get label_id from detection Tensor, last added to this RegionOfInterest
    # @return last added detection Tensor label_id if exists, otherwise None
    def label_id(self) -> int:
        detection = self.detection()
        return detection.label_id() if detection else None

    ## @brief Add new Tensor (inference result) to the RegionOfInterest.
    # @param name Name for the tensor.
    # This function does not take ownership of tensor passed, but only copies its contents
    # @return just created Tensor object, which can be filled with tensor information further
    def add_tensor(self, name: str = "") -> Tensor:
        tensor = libgst.gst_structure_new_empty(name.encode('utf-8'))
        libgstvideo.gst_video_region_of_interest_meta_add_param(self.meta(), tensor)
        return Tensor(tensor)

    ## @brief Get VideoRegionOfInterestMeta containing bounding box information and tensors (inference results).
    # Tensors are represented as GstStructures added to GstVideoRegionOfInterestMeta.params
    # @return VideoRegionOfInterestMeta containing bounding box and tensors (inference results)
    def meta(self) -> VideoRegionOfInterestMeta:
        return self.__roi_meta

    ## @brief Iterate by VideoRegionOfInterestMeta instances attached to buffer
    # @param buffer buffer with GstVideoRegionOfInterestMeta instances attached
    # @return generator for VideoRegionOfInterestMeta instances attached to buffer
    @classmethod
    def _iterate(self, buffer: Gst.Buffer):
        try:
            meta_api = hash(GObject.GType.from_name("GstVideoRegionOfInterestMetaAPI"))
        except:
            return
        gpointer = ctypes.c_void_p()
        while True:
            try:
                value = libgst.gst_buffer_iterate_meta_filtered(hash(buffer), ctypes.byref(gpointer), meta_api)
            except:
                value = None

            if not value:
                return

            roi_meta = ctypes.cast(value, ctypes.POINTER(VideoRegionOfInterestMeta)).contents
            yield RegionOfInterest(roi_meta)

    ## @brief Construct RegionOfInterest instance from VideoRegionOfInterestMeta. After this, RegionOfInterest will
    # obtain all tensors (detection & inference results) from VideoRegionOfInterestMeta
    # @param roi_meta VideoRegionOfInterestMeta containing bounding box information and tensors
    def __init__(self, roi_meta: VideoRegionOfInterestMeta):
        self.__roi_meta = roi_meta

    ## @brief Get region ID
    # @return Region id as an int. Can be a positive or negative integer, but never zero.
    def region_id(self):
        return self.__roi_meta.id
