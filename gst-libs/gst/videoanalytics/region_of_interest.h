/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
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
/**
 * @def GST_VIDEO_REGION_OF_INTEREST_META_ITERATE
 * @brief This macro iterates through GstVideoRegionOfInterestMeta instances for passed buf, retrieving the next
 * GstVideoRegionOfInterestMeta. If state points to NULL, the first GstVideoRegionOfInterestMeta is returned
 * @param buf GstBuffer* of which metadata is iterated and retrieved
 * @param state gpointer* that updates with opaque pointer after macro call.
 * @return GstVideoRegionOfInterestMeta* instance attached to buf
 */
#define GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buf, state)                                                          \
    ((GstVideoRegionOfInterestMeta *)gst_buffer_iterate_meta_filtered(buf, state,                                      \
                                                                      GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE))

#ifdef __cplusplus

#include <cassert>
#include <stdexcept>

#include <string>
#include <vector>

namespace GVA {
/**
 * @brief This class represents region of interest - object describing detection result (bounding box) and containing
 * multiple Tensor objects (inference results) attached by multiple models. For example, it can be region of interest
 * with detected face and recognized age and sex of a person. It can be produced by a pipeline with gvadetect with
 * detection model and two gvaclassify elements with two classification models. Such RegionOfInterest will have bounding
 * box coordinates filled and will have 3 Tensor objects attached - 1 Tensor object with detection result and 2 Tensor
 * objects with classification results coming from 2 classifications
 */
class RegionOfInterest {
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

  public:
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
            GstStructure *s = (GstStructure *)l->data;
            if (not gst_structure_has_name(s, "object_id")) {
                _tensors.emplace_back(s);
                if (_tensors.back().is_detection())
                    _detection = &_tensors.back();
            }
        }
    }

    /**
     * @brief Get number of all Tensor instances added to this RegionOfInterest
     * @return number of all Tensor instances added to this RegionOfInterest
     */
    size_t tensors_number() const {
        return _tensors.size();
    }

    /**
     * @brief Add new tensor (inference result) to this RegionOfInterest with name set. To add detection tensor, set
     * name to "detection"
     * @param name name for the tensor. If name is set to "detection", detection Tensor will be created and set for this
     * RegionOfInterest
     * @return just created Tensor object, which can be filled with tensor information further
     */
    Tensor add_tensor(const std::string &name) {
        GstStructure *tensor_structure = gst_structure_new_empty(name.c_str());
        return add_tensor(tensor_structure);
    }

    /**
     * @brief Add new tensor (inference result) to this RegionOfInterest using existing tensor as basic GstStructure.
     * New Tensor object will derive all the fields from tensor argument. This function takes ownership of tensor
     * GstStructure.
     * @param tensor already created tensor in a form of GstStructure. If it's name set to "detection", detection Tensor
     * will be created and set for this RegionOfInterest. Pass unique heap-allocated GstStructure here and do not
     * free tensor manually after function invoked.
     * @return just created Tensor object, which can be filled with tensor information further
     */
    Tensor add_tensor(GstStructure *tensor) {
        gst_video_region_of_interest_meta_add_param(_gst_meta, tensor);
        _tensors.emplace_back(tensor);
        if (_tensors.back().is_detection())
            _detection = &_tensors.back();

        return _tensors.back();
    }

    /**
     * @brief Get all Tensor instances added to this RegionOfInterest
     * @return vector of Tensor instances added to this RegionOfInterest
     */
    std::vector<Tensor> tensors() const {
        return this->_tensors;
    }

    /**
     * @brief Get Tensor object by index
     * @param index index to get Tensor object by
     * @return Tensor object by index
     */
    Tensor operator[](size_t index) const {
        return _tensors[index];
    }

    /**
     * @brief Get pointer to underlying GstVideoRegionOfInterestMeta containing bounding box information and tensors
     * (inference results). Tensors are represented as GstStructures added to GstVideoRegionOfInterestMeta.params
     * @return pointer to underlying GstVideoRegionOfInterestMeta containing bounding box and tensors (inference
     * results)
     */
    GstVideoRegionOfInterestMeta *meta() const {
        return _gst_meta;
    }

    /**
     * @brief Get confidence from detection Tensor, last added to this RegionOfInterest
     * @return last added detection Tensor confidence if exists, otherwise 0.0
     */
    double confidence() const {
        return _detection ? _detection->confidence() : 0.0;
    }

    /**
     * @brief Returns detection Tensor, last added to this RegionOfInterest. As any other Tensor, returned detection
     * Tensor can contain arbitrary information. If you use RegionOfInterest based on GstVideoRegionOfInterestMeta
     * attached by gvadetect by default, then this Tensor will contain "label_id", "confidence", "x_min", "x_max",
     * "y_min", "y_max" fields. Better way to obtain bounding box information is to use meta().
     * If RegionOfInterest doesn't have detection Tensor, it will be created in-place.
     * @return detection Tensor, empty if there were no detection Tensor objects added to this RegionOfInterest when
     * this method was called
     */
    Tensor detection() {
        if (!_detection) {
            add_tensor("detection");
        }
        return *_detection;
    }

    /**
     * @brief Get label_id from detection Tensor, last added to this RegionOfInterest
     * @return last added detection Tensor label_id if exists, otherwise 0
     */
    int label_id() const {
        return _detection ? _detection->label_id() : 0;
    }

    /**
     * @brief Get class label of this RegionOfInterest
     * @return Class label of this RegionOfInterest
     */
    std::string label() const {
        return std::string(g_quark_to_string(this->meta()->roi_type));
    }

    /**
     * @brief Tensor objects vector iterator
     */
    typedef std::vector<Tensor>::iterator iterator;

    /**
     * @brief Tensor objects vector const iterator
     */
    typedef std::vector<Tensor>::const_iterator const_iterator;

    /**
     * @brief Get iterator pointing to the first Tensor added to this RegionOfInterest
     * @return iterator pointing to the first Tensor added to this RegionOfInterest
     */
    iterator begin() {
        return _tensors.begin();
    }

    /**
     * @brief Get iterator pointing to the past-the-end Tensor added to this RegionOfInterest
     * @return iterator pointing to the past-the-end Tensor added to this RegionOfInterest
     */
    iterator end() {
        return _tensors.end();
    }

    /**
     * @brief Get const iterator pointing to the first Tensor added to this RegionOfInterest
     * @return const iterator pointing to the first Tensor added to this RegionOfInterest
     */
    const_iterator begin() const {
        return _tensors.begin();
    }

    /**
     * @brief Get const iterator pointing to the past-the-end Tensor added to this RegionOfInterest
     * @return const iterator pointing to the past-the-end Tensor added to this RegionOfInterest
     */
    const_iterator end() const {
        return _tensors.end();
    }
};

} // namespace GVA

#endif // #ifdef __cplusplus
