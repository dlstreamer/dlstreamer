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

#include "../metadata/gstanalyticskeypointsmtd.h"
#include "objectdetectionmtdext.h"
#include "tensor.h"

#include <cstdint>
#include <gst/analytics/analytics.h>
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
        if (_gst_meta) {
            return {_gst_meta->x, _gst_meta->y, _gst_meta->w, _gst_meta->h};
        }

        gint x;
        gint y;
        gint w;
        gint h;

        if (!gst_analytics_od_mtd_get_location(const_cast<GstAnalyticsODMtd *>(&_od_meta), &x, &y, &w, &h, nullptr)) {
            throw std::runtime_error("Error when trying to read the location of the RegionOfInterest");
        }
        return {static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
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
     * @brief Get RegionOfInterest bounding box rotation
     * @return Bounding box rotation of the RegionOfInterest
     */
    double rotation() const {
        if (_gst_meta) {
            return _detection ? _detection->get_double("rotation", 0.0) : 0.0;
        }

        gdouble rotation;
        if (!gst_analytics_od_ext_mtd_get_rotation(&_od_ext_meta, &rotation)) {
            throw std::runtime_error("Error when trying to read the rotation of the RegionOfInterest");
        }
        return rotation;
    }

    /**
     * @brief Get RegionOfInterest label
     * @return RegionOfInterest label
     */
    std::string label() const {
        if (_gst_meta) {
            const char *str = g_quark_to_string(_gst_meta->roi_type);
            return std::string(str ? str : "");
        }

        GQuark label = gst_analytics_od_mtd_get_obj_type(const_cast<GstAnalyticsODMtd *>(&_od_meta));
        return label ? g_quark_to_string(label) : "";
    }

    /**
     * @brief Get RegionOfInterest detection confidence (set by gvadetect)
     * @return last added detection Tensor confidence if exists, otherwise 0.0
     */
    double confidence() const {
        if (_gst_meta) {
            return _detection ? _detection->confidence() : 0.0;
        }

        gfloat conf;
        if (!gst_analytics_od_mtd_get_confidence_lvl(const_cast<GstAnalyticsODMtd *>(&_od_meta), &conf)) {
            throw std::runtime_error("Error when trying to read the confidence of the RegionOfInterest");
        }
        return conf;
    }

    /**
     * @brief Get unique id of the RegionOfInterest. The id typically created by gvatrack element.
     * @return Unique id, or zero value if not found
     */
    int32_t object_id() const {
        GstStructure *object_id_struct = nullptr;
        if (_gst_meta) {
            object_id_struct = gst_video_region_of_interest_meta_get_param(_gst_meta, "object_id");
        } else {
            object_id_struct = gst_analytics_od_ext_mtd_get_param(&_od_ext_meta, "object_id");
        }
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
        if (_gst_meta) {
            gst_video_region_of_interest_meta_add_param(_gst_meta, tensor);
        } else {
            gst_analytics_od_ext_mtd_add_param(&_od_ext_meta, tensor);
        }
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
        if (_gst_meta) {
            return _detection ? _detection->label_id() : 0;
        }
        gint label_id;
        if (!gst_analytics_od_ext_mtd_get_class_id(&_od_ext_meta, &label_id)) {
            throw std::runtime_error("Error when trying to read the label id of the RegionOfInterest");
        }
        return label_id;
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

    RegionOfInterest(GstAnalyticsODMtd od_meta, GstAnalyticsODExtMtd od_ext_meta)
        : _gst_meta(nullptr), _detection(nullptr), _od_meta(od_meta), _od_ext_meta(od_ext_meta) {

        GList *params = gst_analytics_od_ext_mtd_get_params(&od_ext_meta);
        _tensors.reserve(g_list_length(params));

        for (GList *l = params; l; l = g_list_next(l)) {
            GstStructure *s = GST_STRUCTURE(l->data);
            if (not gst_structure_has_name(s, "object_id")) {
                _tensors.emplace_back(s);
                if (_tensors.back().is_detection())
                    _detection = &_tensors.back();
            }
        }

        // append tensors converted from metadata
        gpointer state = NULL;
        GstAnalyticsMtd handle;
        while (gst_analytics_relation_meta_get_direct_related(
            od_meta.meta, od_meta.id, GST_ANALYTICS_REL_TYPE_RELATE_TO, GST_ANALYTICS_MTD_TYPE_ANY, &state, &handle)) {
            GstStructure *s = GVA::Tensor::convert_to_tensor(handle);
            if (s != nullptr)
                _tensors.emplace_back(s);
        }
    }

    /**
     * @brief Access RegionOfInterest ID
     * Use this method to get RegionOfInterest ID. ID is generated with "GVA::VideoFrame::add_region()" call.
     * Region ID can be a positive or negative integer, but never zero.
     * @return ID field value
     */
    int region_id() {
        if (_gst_meta) {
            return _gst_meta->id;
        }
        return _od_meta.id;
    }

    /**
     * @brief Set RegionOfInterest label
     * @param label Label to set
     */
    void set_label(std::string label) {
        assert(_gst_meta != nullptr);
        _gst_meta->roi_type = g_quark_from_string(label.c_str());
    }

    /**
     * @brief Set object ID
     * @param id ID to set
     */
    void set_object_id(int32_t id) {
        if (_gst_meta) {
            GstStructure *object_id = gst_video_region_of_interest_meta_get_param(_gst_meta, "object_id");
            if (object_id) {
                gst_structure_set(object_id, "id", G_TYPE_INT, id, NULL);
            } else {
                object_id = gst_structure_new("object_id", "id", G_TYPE_INT, id, NULL);
                gst_video_region_of_interest_meta_add_param(_gst_meta, object_id);
            }
            return;
        }
        GstStructure *object_id = gst_analytics_od_ext_mtd_get_param(&_od_ext_meta, "object_id");
        if (object_id) {
            gst_structure_set(object_id, "id", G_TYPE_INT, id, NULL);
        } else {
            object_id = gst_structure_new("object_id", "id", G_TYPE_INT, id, NULL);
            gst_analytics_od_ext_mtd_add_param(&_od_ext_meta, object_id);
        }
    }

    GList *get_params() const {
        if (_gst_meta) {
            return _gst_meta->params;
        }
        return gst_analytics_od_ext_mtd_get_params(&_od_ext_meta);
    }

    GstStructure *get_param(const char *name) const {
        if (_gst_meta) {
            return gst_video_region_of_interest_meta_get_param(_gst_meta, name);
        }
        return gst_analytics_od_ext_mtd_get_param(&_od_ext_meta, name);
    }

    void add_param(GstStructure *s) {
        if (_gst_meta) {
            gst_video_region_of_interest_meta_add_param(_gst_meta, s);
            return;
        }
        gst_analytics_od_ext_mtd_add_param(&_od_ext_meta, s);
    }

    /**
     * @brief Internal function, don't use or use with caution.
     * @return pointer to underlying GstVideoRegionOfInterestMeta
     */
    GstVideoRegionOfInterestMeta *_meta() const {
        assert(_gst_meta != nullptr);
        return _gst_meta;
    }

  protected:
    /**
     * @brief GstVideoRegionOfInterestMeta containing fields filled with detection result (produced by gvadetect
     * element in Gstreamer pipeline) and all the additional tensors, describing detection and other inference
     * results (produced by gvainference, gvadetect, gvaclassify in Gstreamer pipeline)
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

    /**
     * @brief handle containing data required to use gst_analytics_od_mtd APIs, to retrieve analytics data related
     * to that region of interest.
     */
    GstAnalyticsODMtd _od_meta;
    GstAnalyticsODExtMtd _od_ext_meta;
};

} // namespace GVA
