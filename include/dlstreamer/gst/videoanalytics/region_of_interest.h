/*******************************************************************************
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

/**
 * @file region_of_interest.h
 * @brief This file contains GVA::RegionOfInterest class to control region of interest for particular GVA::VideoFrame
 * with GVA::Tensor instances added
 */

#pragma once

#include "tensor.h"

#include <gst/gst.h>
#include <gst/video/gstvideometa.h>

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

namespace GVA {

/**
 * @brief Template structure for rectangle containing x, y, w, h fields
 */
template <typename T>
struct Rect {
    T x, y, w, h;
};

/**
 * @brief This class represents region of interest - object describing detection result (bounding box) and containing
 * multiple Tensor objects (inference results) attached by multiple models. For example, it can be region of interest
 * with detected face and recognized age and sex of a person. It can be produced by a pipeline with gvadetect with
 * detection model and two gvaclassify elements with two classification models. Such RegionOfInterest will have bounding
 * box coordinates filled and will have 3 Tensor objects attached - 1 Tensor object with detection result and 2 Tensor
 * objects with classification results coming from 2 classifications
 */
class RegionOfInterest {
  public:
    /**
     * @brief Get bounding box of the RegionOfInterest as pixel coordinates in original image
     * @return Bounding box coordinates of the RegionOfInterest
     */
    Rect<uint32_t> rect() const {
        return {_gst_meta->x, _gst_meta->y, _gst_meta->w, _gst_meta->h};
    }

    /**
     * @brief Get bounding box of the RegionOfInterest as normalized coordinates in the range [0, 1]
     * @return Bounding box coordinates of the RegionOfInterest
     */
    Rect<double> normalized_rect() {
        Tensor det = detection();
        return {det.get_double("x_min"), det.get_double("y_min"), det.get_double("x_max") - det.get_double("x_min"),
                det.get_double("y_max") - det.get_double("y_min")};
    }

    /**
     * @brief Get RegionOfInterest bounding box radius
     * @return Bounding box radius of the RegionOfInterest
     */
    double radius() const {
        return _detection ? _detection->get_double("radius", 0.0) : 0.0;
    }

    /**
     * @brief Get RegionOfInterest label
     * @return RegionOfInterest label
     */
    std::string label() const {
        const char *str = g_quark_to_string(_gst_meta->roi_type);
        return std::string(str ? str : "");
    }

    /**
     * @brief Get RegionOfInterest detection confidence (set by gvadetect)
     * @return last added detection Tensor confidence if exists, otherwise 0.0
     */
    double confidence() const {
        return _detection ? _detection->confidence() : 0.0;
    }

    /**
     * @brief Get unique id of the RegionOfInterest. The id typically created by gvatrack element.
     * @return Unique id, or zero value if not found
     */
    int32_t object_id() const {
        GstStructure *object_id_struct = gst_video_region_of_interest_meta_get_param(_gst_meta, "object_id");
        if (!object_id_struct)
            return 0;
        int id = 0;
        gst_structure_get_int(object_id_struct, "id", &id);
        return id;
    }

    /**
     * @brief Get all Tensor instances added to this RegionOfInterest
     * @return vector of Tensor instances added to this RegionOfInterest
     */
    std::vector<Tensor> tensors() const {
        return this->_tensors;
    }

    /**
     * @brief Add new tensor (inference result) to this RegionOfInterest with name set. To add detection tensor, set
     * name to "detection"
     * @param name name for the tensor. If name is set to "detection", detection Tensor will be created and set for this
     * RegionOfInterest
     * @return just created Tensor object, which can be filled with tensor information further
     */
    Tensor add_tensor(const std::string &name) {
        GstStructure *tensor = gst_structure_new_empty(name.c_str());
        gst_video_region_of_interest_meta_add_param(_gst_meta, tensor);
        _tensors.emplace_back(tensor);
        if (_tensors.back().is_detection())
            _detection = &_tensors.back();

        return _tensors.back();
    }

    /**
     * @brief Returns detection Tensor, last added to this RegionOfInterest. As any other Tensor, returned detection
     * Tensor can contain arbitrary information. If you use RegionOfInterest based on GstVideoRegionOfInterestMeta
     * attached by gvadetect by default, then this Tensor will contain "label_id", "confidence", "x_min", "x_max",
     * "y_min", "y_max" fields.
     * If RegionOfInterest doesn't have detection Tensor, it will be created in-place.
     * @return detection Tensor, empty if there were no detection Tensor objects added to this RegionOfInterest when
     * this method was called
     */
    Tensor detection() {
        if (!_detection) {
            add_tensor("detection");
        }
        return _detection ? *_detection : nullptr;
    }

    /**
     * @brief Get label_id from detection Tensor, last added to this RegionOfInterest
     * @return last added detection Tensor label_id if exists, otherwise 0
     */
    int label_id() const {
        return _detection ? _detection->label_id() : 0;
    }

    /**
     * @brief Construct RegionOfInterest instance from GstVideoRegionOfInterestMeta. After this, RegionOfInterest will
     * obtain all tensors (detection & inference results) from GstVideoRegionOfInterestMeta
     * @param meta GstVideoRegionOfInterestMeta containing bounding box information and tensors
     */
    RegionOfInterest(GstVideoRegionOfInterestMeta *meta) : _gst_meta(meta), _detection(nullptr) {
        if (not _gst_meta)
            throw std::invalid_argument("GVA::RegionOfInterest: meta is nullptr");

        _tensors.reserve(g_list_length(meta->params));

        for (GList *l = meta->params; l; l = g_list_next(l)) {
            GstStructure *s = GST_STRUCTURE(l->data);
            if (not gst_structure_has_name(s, "object_id")) {
                _tensors.emplace_back(s);
                if (_tensors.back().is_detection())
                    _detection = &_tensors.back();
            }
        }
    }

    /**
     * @brief Access RegionOfInterest ID
     * Use this method to get RegionOfInterest ID. ID is generated with "GVA::VideoFrame::add_region()" call.
     * Region ID can be a positive or negative integer, but never zero.
     * @return ID field value
     */
    int region_id() {
        return _gst_meta->id;
    }

    /**
     * @brief Set RegionOfInterest label
     * @param label Label to set
     */
    void set_label(std::string label) {
        _gst_meta->roi_type = g_quark_from_string(label.c_str());
    }

    /**
     * @brief Set object ID
     * @param id ID to set
     */
    void set_object_id(int32_t id) {
        GstStructure *object_id = gst_video_region_of_interest_meta_get_param(_gst_meta, "object_id");
        if (object_id) {
            gst_structure_set(object_id, "id", G_TYPE_INT, id, NULL);
        } else {
            object_id = gst_structure_new("object_id", "id", G_TYPE_INT, id, NULL);
            gst_video_region_of_interest_meta_add_param(_gst_meta, object_id);
        }
    }

    /**
     * @brief Internal function, don't use or use with caution.
     * @return pointer to underlying GstVideoRegionOfInterestMeta
     */
    GstVideoRegionOfInterestMeta *_meta() const {
        return _gst_meta;
    }

  protected:
    /**
     * @brief GstVideoRegionOfInterestMeta containing fields filled with detection result (produced by gvadetect element
     * in Gstreamer pipeline) and all the additional tensors, describing detection and other inference results (produced
     * by gvainference, gvadetect, gvaclassify in Gstreamer pipeline)
     */
    GstVideoRegionOfInterestMeta *_gst_meta;
    /**
     * @brief vector of Tensor objects added to this RegionOfInterest (describing detection & inference results),
     * obtained from GstVideoRegionOfInterestMeta
     */
    std::vector<Tensor> _tensors;
    /**
     * @brief last added detection Tensor instance, defined as Tensor with name set to "detection"
     */
    Tensor *_detection;
};

} // namespace GVA
