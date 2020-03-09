# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

## @file region_of_interest.py
#  @brief This file contains gstgva.region_of_interest.RegionOfInterest class to control region of interest for particular gstgva.video_frame.VideoFrame with gstgva.tensor.Tensor instances attached

import ctypes
import numpy
from typing import List

from .tensor import Tensor
from .video_region_of_interest_meta import VideoRegionOfInterestMeta
from .util import libgst, libgobject, libgstvideo, GLIST_POINTER

from gi.repository import GstVideo, GLib, GObject, Gst
import gi
gi.require_version('GstVideo', '1.0')
gi.require_version('GLib', '2.0')
gi.require_version('Gst', '1.0')


## @brief This class represents region of interest - object describing detection result (bounding box) and containing
# multiple Tensor objects (inference results) attached by multiple models. For example, it can be region of interest with detected face and recognized age
# and sex of a person. It can be produced by a pipeline with gvadetect with detection model and two gvaclassify
# elements with two classification models. Such RegionOfInterest will have bounding box coordinates filled and will have 3 Tensor objects
# attached - 1 Tensor object with detection result and 2 Tensor objects with classification results coming from 2 classifications
class RegionOfInterest(object):
    ## @brief Construct RegionOfInterest instance from VideoRegionOfInterestMeta. After this, RegionOfInterest will
    # obtain all tensors (detection & inference results) from VideoRegionOfInterestMeta
    # @param roi_meta VideoRegionOfInterestMeta containing bounding box information and tensors
    def __init__(self, roi_meta: VideoRegionOfInterestMeta):
        self.__roi_meta = roi_meta

    ## @brief Add new Tensor (inference result) to this RegionOfInterest using name or existing tensor as basic Gst.Structure.
    # New Tensor object will derive all the fields from tensor argument
    # @param name name for the tensor. It is used only if tensor argument is not passed. If name is set to "detection",
    # detection Tensor will be created and added to this RegionOfInterest
    # @param tensor already created tensor in a form of Gst.Structure. If it's name set to "detection", detection Tensor
    # will be created and added to this RegionOfInterest. 
    # This function does not take ownership of tensor passed, but only copies its contents
    # @return just created Tensor object, which can be filled with tensor information further
    def add_tensor(self, name: str = "", tensor: Gst.Structure = None) -> Tensor:
        if not tensor:
            tensor = libgst.gst_structure_new_empty(name.encode('utf-8'))
        else:
            tensor = libgst.gst_structure_copy(hash(tensor))

        libgstvideo.gst_video_region_of_interest_meta_add_param(self.meta(), tensor)
        return Tensor(tensor)

    ## @brief Get VideoRegionOfInterestMeta containing bounding box information and tensors (inference results).
    # Tensors are represented as GstStructures added to GstVideoRegionOfInterestMeta.params
    # @return VideoRegionOfInterestMeta containing bounding box and tensors (inference results)
    def meta(self) -> VideoRegionOfInterestMeta:
        return self.__roi_meta

    ## @brief Returns detection Tensor, last added to this RegionOfInterest. As any other Tensor, returned detection
    # Tensor can contain arbitrary information. If you use RegionOfInterest based on VideoRegionOfInterestMeta
    # attached by gvadetect by default, then this Tensor will contain "label_id", "confidence", "x_min", "x_max",
    # "y_min", "y_max" fields. Better way to obtain bounding box information is to use meta().
    # If RegionOfInterest doesn't have detection Tensor, it will be created in-place
    # @return detection Tensor, empty if there were no detection Tensor objects added to this RegionOfInterest when this method was called
    def detection(self) -> Tensor:
        # reversed to be compatible with C++ API impl
        detection = next((tensor for tensor in reversed(self) if tensor.is_detection()), [None])
        if not detection:
            detection = self.add_tensor("detection")

        assert(detection != None)
        return detection

    ## @brief Get confidence from detection Tensor, last added to this RegionOfInterest
    # @return last added detection Tensor confidence if exists, otherwise None
    def confidence(self) -> float:
        return self.detection().confidence() if self.detection() else None

    ## @brief Get label_id from detection Tensor, last added to this RegionOfInterest
    # @return last added detection Tensor label_id if exists, otherwise None
    def label_id(self) -> int:
        return self.detection().label_id() if self.detection() else None
    
    ## @brief Get class label of this RegionOfInterest
    #  @return Class label of this RegionOfInterest
    def label(self) -> str:
        return self.meta().get_roi_type()

    ## @brief Get all Tensor instances added to this RegionOfInterest
    # @return list of Tensor instances added to this RegionOfInterest
    def tensors(self) -> List[Tensor]:
        return [tensor for tensor in self]

    ## @brief Get number of all Tensor instances added to this RegionOfInterest
    # @return number of all Tensor instances added to this RegionOfInterest
    def tensors_number(self) -> int:
        return len(self.tensors())

    ## @brief Get number of all Tensor instances added to this RegionOfInterest
    # @return number of all Tensor instances added to this RegionOfInterest
    def __len__(self) -> int:
        return self.tensors_number()

    ## @brief Get Tensor object by index
    # @param index index to get Tensor object by
    # @return Tensor object by index
    def __getitem__(self, index: int) -> Tensor:
        return self.tensors()[index]

    ## @brief Iterate by all Tensor instances added to this RegionOfInterest
    # @return Generator for all Tensor instances added to this RegionOfInterest
    def __iter__(self) -> Tensor:
        param = self.meta()._params
        while param:
            tensor_structure = param.contents.data
            # "object_id" is used to store ROI id for tracking
            if not libgst.gst_structure_has_name(tensor_structure, "object_id".encode('utf-8')):
                yield Tensor(tensor_structure)
            param = param.contents.next
