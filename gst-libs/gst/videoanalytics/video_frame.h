/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

/**
 * @file video_frame.h
 * @brief This file contains GVA::VideoFrame class to control particular inferenced frame and attached
 * GVA::RegionOfInterest and GVA::Tensor instances. It also provides GVA::MappedMat class to access image information of
 * GVA::VideoFrame
 */

#pragma once

#include <algorithm>
#include <assert.h>
#include <functional>
#include <gst/gstbuffer.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/video.h>
#include <memory>
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#include "gva_json_meta.h"
#include "gva_tensor_meta.h"
#include "region_of_interest.h"

namespace GVA {

/**
 * @brief This class represents mapped data from GstBuffer in matrix form using cv::Mat
 */
class MappedMat {
  private:
    GstBuffer *buffer;
    GstMapInfo map_info;
    cv::Mat cv_mat;

    MappedMat();
    MappedMat(const MappedMat &);
    MappedMat &operator=(const MappedMat &);

  public:
    /**
     * @brief Construct MappedMat instance from GstBuffer and GstVideoInfo
     * @param buffer GstBuffer* containing image of interest
     * @param video_info GstVideoInfo* containing video information
     * @param flag GstMapFlags flags used when mapping memory
     */
    MappedMat(GstBuffer *buffer, const GstVideoInfo *video_info, GstMapFlags flag = GST_MAP_READ) : buffer(buffer) {
        if (!gst_buffer_map(buffer, &map_info, flag))
            throw std::runtime_error("GVA::MappedMat: Could not map buffer to system memory");

        GstVideoFormat format = video_info->finfo->format;
        int stride = video_info->stride[0];

        switch (format) {
        case GST_VIDEO_FORMAT_BGR:
            this->mat() = cv::Mat(cv::Size(video_info->width, video_info->height), CV_8UC3,
                                  reinterpret_cast<char *>(map_info.data), stride);
            break;
        case GST_VIDEO_FORMAT_NV12:
            this->mat() = cv::Mat(cv::Size(video_info->width, static_cast<int32_t>(video_info->height * 1.5f)), CV_8UC1,
                                  reinterpret_cast<char *>(map_info.data), stride);
            break;
        case GST_VIDEO_FORMAT_BGRA:
        case GST_VIDEO_FORMAT_BGRx:
            this->mat() = cv::Mat(cv::Size(video_info->width, video_info->height), CV_8UC4,
                                  reinterpret_cast<char *>(map_info.data), stride);
            break;

        default:
            throw std::runtime_error("GVA::MappedMat: Unsupported format");
        }
    }

    ~MappedMat() {
        if (buffer != nullptr)
            gst_buffer_unmap(buffer, &map_info);
    }

    /**
     * @brief Get mapped data from GstBuffer as a cv::Mat
     * @return data wrapped by cv::Mat
     */
    cv::Mat &mat() {
        return cv_mat;
    }
};

/**
 * @brief This class represents video frame - object for working with RegionOfInterest and Tensor objects which
 * belong to this video frame (image). RegionOfInterest describes detected object (bounding boxes) and its Tensor
 * objects (inference results on RegionOfInterest level). Tensor describes inference results on VideoFrame level.
 * VideoFrame also provides access to underlying GstBuffer and GstVideoInfo describing frame's video information (such
 * as image width, height, channels, strides, etc.). You also can get cv::Mat object representing this video frame.
 */
class VideoFrame {
  protected:
    /**
     * @brief GstBuffer with inference results metadata attached (Gstreamer pipeline's GstBuffer, which is output of GVA
     * inference elements, such as gvadetect, gvainference, gvaclassify)
     */
    GstBuffer *buffer;

    /**
     * @brief GstVideoInfo containing actual video information for this VideoFrame
     */
    std::unique_ptr<GstVideoInfo, std::function<void(GstVideoInfo *)>> info;

    /**
     * @brief vector of RegionOfInterest objects for this VideoFrame. These regions have GstVideoRegionOfInterestMeta
     * metadata type and they are produced by gvadetect and updated with classification tensors by gvaclassify
     */
    std::vector<RegionOfInterest> _regions;

    /**
     * @brief vector of tensors (inference results) for this VideoFrame. These tensors have GstGVATensorMeta metadata
     * type and they are produced by gvainference element
     */
    std::vector<Tensor> _tensors;

  public:
    /**
     * @brief Construct VideoFrame instance from GstBuffer and GstVideoInfo. This is preferred way of creating
     * VideoFrame
     * @param buffer GstBuffer* to which metadata is attached and retrieved
     * @param info GstVideoInfo* containing video information
     */
    VideoFrame(GstBuffer *buffer, GstVideoInfo *info)
        : buffer(buffer), info(gst_video_info_copy(info), gst_video_info_free) {
        if (not buffer or not info) {
            throw std::invalid_argument("GVA::VideoFrame: buffer or info nullptr");
        }
        init();
    }

    /**
     * @brief Construct VideoFrame instance from GstBuffer and GstCaps
     * @param buffer GstBuffer* to which metadata is attached and retrieved
     * @param caps GstCaps* from which video information is obtained
     */
    VideoFrame(GstBuffer *buffer, const GstCaps *caps) : buffer(buffer) {
        if (not buffer or not caps) {
            throw std::invalid_argument("GVA::VideoFrame: buffer or caps nullptr");
        }
        info = std::unique_ptr<GstVideoInfo, std::function<void(GstVideoInfo *)>>(gst_video_info_new(),
                                                                                  gst_video_info_free);
        if (!gst_video_info_from_caps(info.get(), caps)) {
            throw std::runtime_error("GVA::VideoFrame: gst_video_info_from_caps failed");
        }

        init();
    }

    /**
     * @brief Construct VideoFrame instance from GstBuffer. Video information will be obtained from buffer. This is
     * not recommended way of creating VideoFrame, because it relies on GstVideoMeta which can be absent for the
     * buffer
     * @param buffer GstBuffer* to which metadata is attached and retrieved
     */
    VideoFrame(GstBuffer *buffer) : buffer(buffer) {
        if (not buffer)
            throw std::invalid_argument("GVA::VideoFrame: buffer is nullptr");

        GstVideoMeta *meta = video_meta();
        if (not meta)
            throw std::logic_error("GVA::VideoFrame: video_meta() is nullptr");

        info = std::unique_ptr<GstVideoInfo, std::function<void(GstVideoInfo *)>>(gst_video_info_new(),
                                                                                  gst_video_info_free);
        if (not info.get())
            throw std::logic_error("GVA::VideoFrame: gst_video_info_new() failed");

        info->width = meta->width;
        info->height = meta->height;
        memcpy(info->stride, meta->stride, sizeof(meta->stride));

        init();
    }

    /**
     * @brief Get video metadata of buffer
     * @return GstVideoMeta of buffer, nullptr if no GstVideoMeta available
     */
    GstVideoMeta *video_meta() {
        return gst_buffer_get_video_meta(buffer);
    }

    /**
     * @brief Get GstVideoInfo of this VideoFrame. This is preferrable way of getting any image information
     * @return GstVideoInfo of this VideoFrame
     */
    GstVideoInfo *video_info() {
        return info.get();
    }

    /**
     * @brief Get RegionOfInterest objects attached to VideoFrame
     * @return vector of RegionOfInterest objects attached to VideoFrame
     */
    std::vector<RegionOfInterest> regions() {
        return _regions;
    }

    /**
     * @brief Get RegionOfInterest objects attached to VideoFrame
     * @return vector of RegionOfInterest objects attached to VideoFrame
     */
    const std::vector<RegionOfInterest> regions() const {
        return _regions;
    }

    /**
     * @brief Get Tensor objects attached to VideoFrame
     * @return vector of Tensor objects attached to VideoFrame
     */
    std::vector<Tensor> tensors() {
        return _tensors;
    }

    /**
     * @brief  Create GstStructure containing specified vector of labels to be passed to add_region() if needed
     * This function transfers ownership to heap-allocated GstStructure to caller
     * @param labels vector of label strings used for detection model training
     * @return GstStructure containing specified list of labels
     */
    static GstStructure *create_labels_structure(const std::vector<std::string> &labels) {
        GValueArray *arr = g_value_array_new(labels.size());
        GValue gvalue = G_VALUE_INIT;
        g_value_init(&gvalue, G_TYPE_STRING);
        for (std::string label : labels) {
            g_value_set_string(&gvalue, label.c_str());
            g_value_array_append(arr, &gvalue);
        }
        GstStructure *labels_struct = gst_structure_new_empty("labels_struct");
        gst_structure_set_array(labels_struct, "labels", arr);
        return labels_struct;
    }

    /**
     * @brief Get Tensor objects attached to VideoFrame
     * @return vector of Tensor objects attached to VideoFrame
     */
    const std::vector<Tensor> tensors() const {
        return _tensors;
    }

    /**
     * @brief Attach RegionOfInterest to this VideoFrame. This function takes ownership of region_tensor, if passed
     * This int version of `add_region` is implemented to preserve exact values for int coordinates
     * @param x x coordinate of the upper left corner of bounding box (in pixels)
     * @param y y coordinate of the upper left corner of bounding box (in pixels)
     * @param w bounding box width (in pixels)
     * @param h bounding box height (in pixels)
     * @param label_id bounding box label id
     * @param confidence detection confidence
     * @param region_tensor base tensor for detection Tensor which will be added to this new
     * RegionOfInterest. If you want this detection Tensor to have textual representation of label
     * (see Tensor::label()), you pass here GstStructure with "labels" GValueArray containing
     * label strings used for detection model training. Please see VideoFrame Python API documentation & implementation
     * for example
     * This function takes ownership of region_tensor passed. Pass unique heap-allocated GstStructure here and do not
     * free region_tensor manually after function invoked
     * @return new RegionOfInterest instance
     */
    RegionOfInterest add_region(int x, int y, int w, int h, int label_id, double confidence = 0.0,
                                GstStructure *region_tensor = nullptr) {
        if (!this->is_bounded(x, y, w, h)) {
            int x_init = x, y_init = y, w_init = w, h_init = h;
            clip(x, y, w, h);
            GST_DEBUG("ROI coordinates {x, y, w, h} are out of image borders and will be clipped: [%d, %d, %d, %d] "
                      "-> [%d, %d, %d, %d]",
                      x_init, y_init, w_init, h_init, x, y, w, h);
        }

        const gchar *label = "";
        get_label_by_label_id(region_tensor, label_id, &label);

        GstVideoRegionOfInterestMeta *meta = gst_buffer_add_video_region_of_interest_meta(buffer, label, x, y, w, h);

        if (not region_tensor)
            region_tensor = gst_structure_new_empty("detection");
        else
            gst_structure_set_name(region_tensor, "detection"); // make sure we're about to add detection Tensor

        gst_structure_set(region_tensor, "label_id", G_TYPE_INT, label_id, "confidence", G_TYPE_DOUBLE, confidence,
                          "x_min", G_TYPE_DOUBLE, (double)x / info->width, "x_max", G_TYPE_DOUBLE,
                          (double)(x + w) / info->width, "y_min", G_TYPE_DOUBLE, (double)y / info->height, "y_max",
                          G_TYPE_DOUBLE, (double)(y + h) / info->height, NULL);

        _regions.emplace_back(meta);
        _regions.back().add_tensor(region_tensor);
        // region_tensor will be freed along with GstVideoRegionOfInterestMeta
        return _regions.back();
    }

    /**
     * @brief Attach RegionOfInterest to this VideoFrame. This function takes ownership of region_tensor, if passed
     * @param x x coordinate of the upper left corner of bounding box (in [0,1] interval)
     * @param y y coordinate of the upper left corner of bounding box (in [0,1] interval)
     * @param w bounding box width (in [0,1-x] interval)
     * @param h bounding box height (in [0,1-y] interval)
     * @param label_id bounding box label id
     * @param confidence detection confidence
     * @param region_tensor base tensor for detection Tensor which will be added to this new
     * RegionOfInterest. If you want this detection Tensor to have textual representation of label
     * (see Tensor::label()), you pass here GstStructure with "labels" GValueArray containing
     * label strings used for detection model training. Please see VideoFrame Python API documentation & implementation
     * for example
     * This function takes ownership of region_tensor passed. Pass unique heap-allocated GstStructure here and do not
     * free region_tensor manually after function invoked
     * @return new RegionOfInterest instance
     */
    RegionOfInterest add_region(double x, double y, double w, double h, int label_id, double confidence = 0.0,
                                GstStructure *region_tensor = nullptr) {
        return this->add_region((int)(x * info->width), (int)(y * info->height), (int)(w * info->width),
                                (int)(h * info->height), label_id, confidence, region_tensor);
    }

    /**
     * @brief Attach empty Tensor to this VideoFrame
     * @return new Tensor instance
     */
    Tensor add_tensor() {
        GstGVATensorMeta *tensor_meta = GST_GVA_TENSOR_META_ADD(buffer);
        _tensors.emplace_back(tensor_meta->data);
        return _tensors.back();
    }

    /**
     * @brief Get messages attached to this VideoFrame
     * @return messages attached to this VideoFrame
     */
    std::vector<std::string> messages() {
        std::vector<std::string> json_messages;
        GstGVAJSONMeta *meta = NULL;
        gpointer state = NULL;
        while ((meta = GST_GVA_JSON_META_ITERATE(buffer, &state))) {
            json_messages.emplace_back(get_json_message(meta));
        }
        return json_messages;
    }

    /**
     * @brief Attach message to this VideoFrame
     * @param message message to attach to this VideoFrame
     */
    void add_message(const std::string &message) {
        GstGVAJSONMeta *json_meta = GST_GVA_JSON_META_ADD(buffer);
        json_meta->message = strdup(message.c_str());
    }

    /**
     * @brief Remove RegionOfInterest with the specified index
     * @param index index of the RegionOfInterest
     */
    void pop_region(size_t index) {
        // TODO: unittest it
        if (index >= _regions.size())
            throw std::out_of_range("GVA::VideoFrame: RegionOfInterest index is out of range");

        if (not _regions[index].meta())
            throw std::runtime_error("GVA::VideoFrame: Underlying GstVideoRegionOfInterestMeta pointer is NULL for "
                                     "RegionOfInterest at index " +
                                     std::to_string(index) + " of this VideoFrame");

        if (not gst_buffer_remove_meta(buffer, (GstMeta *)_regions[index].meta()))
            throw std::runtime_error(
                "GVA::VideoFrame: Underlying GstVideoRegionOfInterestMeta for RegionOfInterest at index " +
                std::to_string(index) + " doesn't belong to this VideoFrame");
        _regions.erase(_regions.begin() + index);
    }

    /**
     * @brief Remove the last RegionOfInterest
     */
    void pop_region() {
        size_t last_idx = _regions.size() - 1;
        pop_region(last_idx);
    }

    /**
     * @brief Get buffer data wrapped by MappedMat
     * @return unique pointer to an instance of MappedMat
     */
    std::unique_ptr<MappedMat> data(GstMapFlags flag = GST_MAP_READWRITE) {
        return std::unique_ptr<MappedMat>(new MappedMat(buffer, info.get(), flag));
    }

  private:
    // TODO: move to C and use in Python via ctypes
    static bool get_label_by_label_id(GstStructure *region_tensor, int label_id, const gchar **out_label) {
        *out_label = "";
        if (region_tensor and gst_structure_has_field(region_tensor, "labels")) {
            GValueArray *labels = nullptr;
            gst_structure_get_array(region_tensor, "labels", &labels);
            if (labels && label_id >= 0 && label_id < (gint)labels->n_values) {
                *out_label = g_value_get_string(labels->values + label_id);
                return true;
            }
        }
        return false;
    }

    void clip(int &x, int &y, int &w, int &h) {
        x = (x < 0) ? 0 : (x > info->width) ? info->width : x;
        y = (y < 0) ? 0 : (y > info->height) ? info->height : y;
        w = (w < 0) ? 0 : (x + w > info->width) ? info->width - x : w;
        h = (h < 0) ? 0 : (y + h > info->height) ? info->height - y : h;
    }

    bool is_bounded(int x, int y, int w, int h) {
        return (x >= 0) and (y >= 0) and (w >= 0) and (h >= 0) and (x + w <= info->width) and (y + h <= info->height);
    }

    void init_regions() {
        GstVideoRegionOfInterestMeta *meta = NULL;
        gpointer state = NULL;
        while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state)))
            _regions.emplace_back(meta);
    }

    void init_tensors() {
        GstGVATensorMeta *meta = NULL;
        gpointer state = NULL;
        while ((meta = GST_GVA_TENSOR_META_ITERATE(buffer, &state)))
            _tensors.emplace_back(meta->data);
    }

    void init() {
        init_regions();
        init_tensors();
    }
};

} // namespace GVA
