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
from warnings import warn

import gi
gi.require_version('Gst', '1.0')
gi.require_version("GstVideo", "1.0")
gi.require_version('GObject', '2.0')

from gi.repository import GObject, Gst, GstVideo
from .util import VideoRegionOfInterestMeta
from .util import GVATensorMeta
from .util import GVAJSONMeta
from .util import GVAJSONMetaStr
from .region_of_interest import RegionOfInterest
from .tensor import Tensor
from .util import libgst, gst_buffer_data


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

    ## @brief Get video metadata of buffer
    #  @return GstVideo.VideoMeta of buffer, nullptr if no GstVideo.VideoMeta available
    def video_meta(self) -> GstVideo.VideoMeta:
        return GstVideo.buffer_get_video_meta(self.__buffer)

    ## @brief Get GstVideo.VideoInfo of this VideoFrame. This is preferrable way of getting any image information
    #  @return GstVideo.VideoInfo of this VideoFrame
    def video_info(self) -> GstVideo.VideoInfo:
        return self.__video_info

    ## @brief Get RegionOfInterest objects attached to VideoFrame
    #  @return iterator of RegionOfInterest objects attached to VideoFrame
    def regions(self):
        return RegionOfInterest._iterate(self.__buffer)

    ## @brief Get Tensor objects attached to VideoFrame
    #  @return iterator of Tensor objects attached to VideoFrame
    def tensors(self):
        return Tensor._iterate(self.__buffer)

    ## @brief Attach RegionOfInterest to this VideoFrame
    #  @param x x coordinate of the upper left corner of bounding box
    #  @param y y coordinate of the upper left corner of bounding box
    #  @param w bounding box width
    #  @param h bounding box height
    #  @param label object label
    #  @param confidence detection confidence
    #  @param region_tensor base tensor for detection Tensor which will be added to this new
    #  @param normalized if True, input coordinates are assumed to be normalized (in [0,1] interval).
    # If False, input coordinates are assumed to be expressed in pixels (this is behavior by default)
    #  @return new RegionOfInterest instance
    def add_region(self, x, y, w, h, label: str = "", confidence: float = 0.0, normalized: bool = False) -> RegionOfInterest:
        if normalized:
            x = int(x * self.video_info().width)
            y = int(y * self.video_info().height)
            w = int(w * self.video_info().width)
            h = int(h * self.video_info().height)

        if not self.__is_bounded(x, y, w, h):
            x_init, y_init, w_init, h_init = x, y, w, h
            x, y, w, h = self.__clip(x, y, w, h)
            warn("ROI coordinates [x, y, w, h] are out of image borders and will be clipped: [{}, {}, {}, {}] -> "
                "[{}, {}, {}, {}]".format(x_init, y_init, w_init, h_init, x, y, w, h), stacklevel=2)

        video_roi_meta = GstVideo.buffer_add_video_region_of_interest_meta(self.__buffer, label, x, y, w, h)
        roi = RegionOfInterest(ctypes.cast(hash(video_roi_meta), ctypes.POINTER(VideoRegionOfInterestMeta)).contents)

        tensor = roi.add_tensor("detection")
        tensor['confidence'] = float(confidence)
        tensor['x_min'] = float(x / self.video_info().width)
        tensor['x_max'] = float((x + w) / self.video_info().width)
        tensor['y_min'] = float(y / self.video_info().height)
        tensor['y_max'] = float((y + h) / self.video_info().height)

        return roi

    ## @brief Attach empty Tensor to this VideoFrame
    #  @return new Tensor instance
    def add_tensor(self) -> Tensor:
        tensor_meta = GVATensorMeta.add_tensor_meta(self.__buffer)
        if tensor_meta:
            return Tensor(tensor_meta.data)
        return None

    ## @brief Get messages attached to this VideoFrame
    #  @return messages attached to this VideoFrame
    def messages(self) -> List[str]:
        return [json_meta.get_message() for json_meta in GVAJSONMeta.iterate(self.__buffer)]

    ## @brief Attach message to this VideoFrame
    #  @param message message to attach to this VideoFrame
    def add_message(self, message: str):
        GVAJSONMeta.add_json_meta(self.__buffer, message)

    ## @brief Remove message from this VideoFrame
    #  @param message message to remove
    def remove_message(self, message: str):
        if not isinstance(message,GVAJSONMetaStr) or not GVAJSONMeta.remove_json_meta(self.__buffer, message.meta):
            raise RuntimeError("VideoFrame: message doesn't belong to this VideoFrame")

    ## @brief Remove region with the specified index
    #  @param roi Region to remove
    def remove_region(self, roi) -> None:
        if not libgst.gst_buffer_remove_meta(hash(self.__buffer), ctypes.byref(roi.meta())):
            raise RuntimeError("VideoFrame: Underlying GstVideoRegionOfInterestMeta for RegionOfInterest "
                               "doesn't belong to this VideoFrame")

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
                warn("Size of buffer's data: {}, and requested size: {}\n"
                     "Let to get shape from video meta...".format(
                    len(data), h * w * bytes_per_pix), stacklevel=2)
                meta = self.video_meta()
                if meta:
                    h, w = meta.height, meta.width
                else:
                    warn("Video meta is {}. Can't get shape.".format(meta),
                            stacklevel=2)

            try:
                yield numpy.ndarray((h, w, bytes_per_pix), buffer=data, dtype=numpy.uint8)
            except TypeError as e:
                warn(str(e) + "\nSize of buffer's data: {}, and requested size: {}".format(
                    len(data), h * w * bytes_per_pix), stacklevel=2)
                raise e

    def __is_bounded(self, x, y, w, h):
        return x >= 0 and y >= 0 and w >= 0 and h >= 0 and x + w <= self.__video_info.width and y + h <= self.__video_info.height

    def __clip(self, x, y, w, h):
        frame_width, frame_height = self.__video_info.width, self.__video_info.height

        x, y = min(max(x, 0), frame_width), min(max(y, 0), frame_height)

        w, h = max(w, 0), max(h, 0)
        w = (frame_width - x) if (w + x) > frame_width else w
        h = (frame_height - y) if (h + y) > frame_height else h

        return x, y, w, h

    @staticmethod
    def __get_label_by_label_id(region_tensor: Gst.Structure, label_id: int) -> str:
        if region_tensor and region_tensor.has_field("labels"):
            res = region_tensor.get_array("labels")
            if res[0] and 0 <= label_id < res[1].n_values:
                return res[1].get_nth(label_id)
        return ""
