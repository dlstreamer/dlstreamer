# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

## @file video_frame.py
#  @brief This file contains gstgva.video_frame.VideoFrame class to control particular inferenced frame
# and attached gstgva.region_of_interest.RegionOfInterest and gstgva.tensor.Tensor instances

import ctypes
import numpy
from contextlib import contextmanager
from typing import List

import gi
gi.require_version('Gst', '1.0')
gi.require_version("GstVideo", "1.0")
gi.require_version('GObject', '2.0')

from gi.repository import GObject, Gst, GstVideo
from .video_region_of_interest_meta import VideoRegionOfInterestMeta
from .gva_tensor_meta import GVATensorMeta
from .gva_json_meta import GVAJSONMeta
from .region_of_interest import RegionOfInterest
from .tensor import Tensor
from .util import libgst, libgstva, gst_buffer_data


## @brief This class represents video frame - object for working with RegionOfInterest and Tensor objects which
# belong to this video frame (image). RegionOfInterest describes detected object (bounding boxes) and its Tensor
# objects (inference results on RegionOfInterest level). Tensor describes inference results on VideoFrame level.
# VideoFrame also provides access to underlying GstBuffer and GstVideoInfo describing frame's video information (such
# as image width, height, channels, strides, etc.). You also can get cv::Mat object representing this video frame.
class VideoFrame:
    ## @brief Construct VideoFrame instance from Gst.Buffer and GstVideo.VideoInfo or Gst.Caps.
    #  The preferred way of creating VideoFrame is to use Gst.Buffer and GstVideo.VideoInfo
    #  @param buffer Gst.Buffer to which metadata is attached and retrieved
    #  @param video_info GstVideo.VideoInfo containing video information
    #  @param caps Gst.Caps from which video information is obtained
    def __init__(self, buffer: Gst.Buffer, video_info: GstVideo.VideoInfo = None, caps: Gst.Caps = None):
        self.__buffer = buffer
        self.__video_info = None

        if video_info:
            self.__video_info = video_info
        elif caps:
            self.__video_info = GstVideo.VideoInfo()
            self.__video_info.from_caps(caps)
        elif self.video_meta():
            self.__video_info = GstVideo.VideoInfo()
            self.__video_info.width = self.video_meta().width
            self.__video_info.height = self.video_meta().height

        self.__init_regions()
        self.__init_tensors()

    ## @brief Get video metadata of buffer
    #  @return GstVideo.VideoMeta of buffer, nullptr if no GstVideo.VideoMeta available
    def video_meta(self) -> GstVideo.VideoMeta:
        return GstVideo.buffer_get_video_meta(self.__buffer)

    ## @brief Get GstVideo.VideoInfo of this VideoFrame. This is preferrable way of getting any image information
    #  @return GstVideo.VideoInfo of this VideoFrame
    def video_info(self) -> GstVideo.VideoInfo:
        return self.__video_info

    ## @brief Get RegionOfInterest objects attached to VideoFrame
    #  @return list of RegionOfInterest objects attached to VideoFrame
    def regions(self) -> List[RegionOfInterest]:
        return self.__regions[:]  # copy list

    ## @brief Get Tensor objects attached to VideoFrame
    #  @return list of Tensor objects attached to VideoFrame
    def tensors(self) -> List[Tensor]:
        return self.__tensors[:]  # copy list

    ## @brief Create Gst.Structure containing specified list of labels to be passed to add_region() if needed
    #  @param labels list of label strings used for detection model training
    #  @return Gst.Structure containing specified list of labels
    @staticmethod
    def create_labels_structure(labels: List[str]) -> Gst.Structure:
        arr = GObject.ValueArray.new(len(labels))
        gvalue = GObject.Value()
        gvalue.init(GObject.TYPE_STRING)    
        for label in labels:
            gvalue.set_string(label)
            arr.append(gvalue)

        labels_struct = Gst.Structure.new_empty("labels_struct")
        labels_struct.set_array("labels", arr)
        return labels_struct

    ## @brief Attach RegionOfInterest to this VideoFrame
    #  @param x x coordinate of the upper left corner of bounding box
    #  @param y y coordinate of the upper left corner of bounding box
    #  @param w bounding box width
    #  @param h bounding box height
    #  @param label_id bounding box label id
    #  @param confidence detection confidence
    #  @param region_tensor base tensor for detection Tensor which will be added to this new
    # RegionOfInterest instance. If you want this detection Tensor to have textual representation of label
    # (see Tensor.label()), you pass here Gst.Structure obtained from create_labels_structure()
    #  @param normalized if True, input coordinates are assumed to be normalized (in [0,1] interval).
    # If False, input coordinates are assumed to be expressed in pixels (this is behavior by default)
    #  @return new RegionOfInterest instance
    def add_region(self, x, y, w, h, label_id: int, confidence: float = 0.0,
                   region_tensor: Gst.Structure = None, normalized: bool = False) -> RegionOfInterest:
        if normalized:
            x = int(x * self.video_info().width)
            y = int(y * self.video_info().height)
            w = int(w * self.video_info().width)
            h = int(h * self.video_info().height)

        if not self.__is_bounded(x, y, w, h):
            x_init, y_init, w_init, h_init = x, y, w, h
            x, y, w, h = self.__clip(x, y, w, h)
            # Gst.debug("ROI coordinates [x, y, w, h] are out of image borders and will be clipped: [{}, {}, {}, {}] -> "
            #     "[{}, {}, {}, {}]".format(x_init, y_init, w_init, h_init, x, y, w, h))

        label = self.__get_label_by_label_id(region_tensor, label_id)

        video_roi_meta = GstVideo.buffer_add_video_region_of_interest_meta(
            self.__buffer, label, x, y, w, h)

        if not region_tensor:
            region_tensor = Gst.Structure.new_empty("detection")
        else:
            region_tensor.set_name("detection")  # make sure we're about to add detection Tensor

        region_tensor.set_value('label_id', label_id)
        region_tensor.set_value('confidence', confidence)
        region_tensor.set_value('x_min', x / self.video_info().width)
        region_tensor.set_value('x_max', (x + w) / self.video_info().width)
        region_tensor.set_value('y_min', y / self.video_info().height)
        region_tensor.set_value('y_max', (y + h) / self.video_info().height)

        self.__regions.append(
            RegionOfInterest(ctypes.cast(hash(video_roi_meta), ctypes.POINTER(VideoRegionOfInterestMeta)).contents))
        self.__regions[-1].add_tensor(tensor=region_tensor)
        return self.__regions[-1]

    ## @brief Attach empty Tensor to this VideoFrame
    #  @return new Tensor instance
    def add_tensor(self) -> Tensor:
        tensor_meta = GVATensorMeta.add_tensor_meta(self.__buffer)
        self.__tensors.append(Tensor(tensor_meta.data))
        return self.tensors()[-1]

    ## @brief Get messages attached to this VideoFrame
    #  @return messages attached to this VideoFrame
    def messages(self) -> List[str]:
        return [libgstva.get_json_message(ctypes.byref(json_meta)).decode('utf-8') for json_meta in
                GVAJSONMeta.iterate(self.__buffer)]

    ## @brief Attach message to this VideoFrame
    #  @param message message to attach to this VideoFrame
    def add_message(self, message: str):
        GVAJSONMeta.add_json_meta(self.__buffer, message)

    ## @brief Remove region with the specified index
    #  @param index index of the region. If not specified, the last region will be deleted
    def pop_region(self, index: int = -1) -> None:
        try:
            if not self.__regions[index].meta():
                raise RuntimeError("VideoFrame: Underlying GstVideoRegionOfInterestMeta is None for "
                                   "RegionOfInterest at index {} of this VideoFrame".format(str(index)))
        except IndexError:
            raise IndexError("VideoFrame: RegionOfInterest index {} is out of range".format(str(index)))

        if not libgst.gst_buffer_remove_meta(hash(self.__buffer), ctypes.byref(self.__regions[index].meta())):
            raise RuntimeError("VideoFrame: Underlying GstVideoRegionOfInterestMeta for RegionOfInterest at "
                               "index {} doesn't belong to this VideoFrame".format(str(index)))
        self.__regions.pop(index)

    ## @brief Get buffer data wrapped by numpy.ndarray
    #  @return numpy array instance
    @contextmanager
    def data(self, flag: Gst.MapFlags = Gst.MapFlags.WRITE) -> numpy.ndarray:
        with gst_buffer_data(self.__buffer, flag) as data:
            bytes_per_pix = self.__video_info.finfo.pixel_stride[0]  # pixel stride for 1st plane. works well for for 1-plane formats, like BGR, BGRA, BGRx
            w = self.__video_info.width
            if self.__video_info.finfo.format == GstVideo.VideoFormat.NV12:
                h = int(self.__video_info.height * 1.5)
            elif self.__video_info.finfo.format == GstVideo.VideoFormat.BGR or \
                 self.__video_info.finfo.format == GstVideo.VideoFormat.BGRA or \
                 self.__video_info.finfo.format == GstVideo.VideoFormat.BGRX:
                h = self.__video_info.height
            else:
                raise RuntimeError("VideoFrame.data: Unsupported format")

            if len(data) != h * w * bytes_per_pix:
                Gst.warning("Size of buffer's data: {}, and requested size: {}".format(
                    len(data), h * w * bytes_per_pix))
                Gst.warning("Let to get shape from video meta...")
                meta = self.video_meta()
                if meta:
                    h, w = meta.height, meta.width
                else:
                    Gst.warning(
                        "Video meta is {}. Can't get shape.".format(meta))

            try:
                yield numpy.ndarray((h, w, bytes_per_pix), buffer=data, dtype=numpy.uint8)
            except TypeError as e:
                Gst.error(str(e))
                Gst.error("Size of buffer's data: {}, and requested size: {}".format(
                    len(data), h * w * bytes_per_pix))
                raise e


    def __init_regions(self):
        self.__regions = list()
        for region_meta in VideoRegionOfInterestMeta.iterate(self.__buffer):
            self.__regions.append(RegionOfInterest(region_meta))

    def __init_tensors(self):
        self.__tensors = list()
        for tensor_meta in GVATensorMeta.iterate(self.__buffer):
            self.__tensors.append(Tensor(tensor_meta.data))

    def __is_bounded(self, x, y, w, h):
        return x >= 0 and y >= 0 and w >= 0 and h >= 0 and x + w <= self.__video_info.width and y + h <= self.__video_info.height

    def __clip(self, x, y, w, h):
        frame_width, frame_height = self.video_info().width, self.video_info().height

        x = 0 if x < 0 else frame_width if x > frame_width else x
        y = 0 if y < 0 else frame_height if x > frame_height else y
        w = 0 if w < 0 else frame_width - x if x + w > frame_width else w
        h = 0 if h < 0 else frame_height - y if y + h > frame_height else h

        return x, y, w, h

    @staticmethod
    def __get_label_by_label_id(region_tensor: Gst.Structure, label_id: int) -> str:
        if region_tensor and region_tensor.has_field("labels"):
            res = region_tensor.get_array("labels")
            if res[0] and 0 <= label_id < res[1].n_values:
                return res[1].get_nth(label_id)
        return ""
