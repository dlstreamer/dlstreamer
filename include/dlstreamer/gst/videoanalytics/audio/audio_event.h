/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

/**
 * @file audio_event.h
 * @brief This file contains GVA::AudioEvent class to control audio event for particular GVA::AudioFrame
 * with GVA::Tensor instances added
 */

#pragma once

#include "tensor.h"

#include "gva_audio_event_meta.h"

#include <gst/audio/gstaudiometa.h>
#include <gst/gst.h>

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

namespace GVA {

/**
 * @brief Template structure for audio segment containing start, end fields
 */
template <typename T>
struct Segment {
    T start, end;
};

/**
 * @brief This class represents audio event - object describing audio event detection result (segment) and containing
 * multiple Tensor objects (inference results) attached by multiple models. For example, it can be audio event
 * with detected speech and converts speech to text. It can be produced by a pipeline with gvaaudiodetect with
 * detection model and gvaspeechtotext element with speechtotext model. Such AudioEvent will have start and end
 * timestamps filled and will have 2 Tensor objects attached - 1 Tensor object with detection result and other with
 * speech to text tensor objectresult
 */
class AudioEvent {
  public:
    /**
     * @brief Get Segment of AudioEvent as start and end timestamps, timestamps are presentation time.
     * @return Start and end time of AudioEvent
     */
    Segment<guint64> segment() const {
        return {_gst_meta->start_timestamp, _gst_meta->end_timestamp};
    }

    /**
     * @brief Get AudioEvent label
     * @return AudioEvent label
     */
    std::string label() const {
        const char *str = g_quark_to_string(_gst_meta->event_type);
        return std::string(str ? str : "");
    }

    /**
     * @brief Get AudioEvent detection confidence (set by gvaaudiodetect)
     * @return last added detection Tensor confidence if exists, otherwise 0.0
     */
    double confidence() const {
        return _detection ? _detection->confidence() : 0.0;
    }

    /**
     * @brief Get all Tensor instances added to this AudioEvent
     * @return vector of Tensor instances added to this AudioEvent
     */
    std::vector<Tensor> tensors() const {
        return this->_tensors;
    }

    /**
     * @brief Add new tensor (inference result) to this AudioEvent with name set. To add detection tensor, set
     * name to "detection"
     * @param name name for the tensor. If name is set to "detection", detection Tensor will be created and set for this
     * AudioEvent
     * @return just created Tensor object, which can be filled with tensor information further
     */
    Tensor add_tensor(const std::string &name) {
        if (name.empty())
            throw std::invalid_argument("GVA::AudioEvent: name is empty");
        GstStructure *tensor = gst_structure_new_empty(name.c_str());
        gst_gva_audio_event_meta_add_param(_gst_meta, tensor);
        _tensors.emplace_back(tensor);
        if (_tensors.back().is_detection())
            _detection = &_tensors.back();

        return _tensors.back();
    }

    /**
     * @brief Returns detection Tensor, last added to this AudioEvent. As any other Tensor, returned detection
     * Tensor can contain arbitrary information. If you use AudioEvent based on GstGVAAudioEventMeta
     * attached by gvaaudiodetect by default, then this Tensor will contain "label_id", "confidence", "start_timestamp",
     * "end_timestamp" fields.
     * If AudioEvent doesn't have detection Tensor, it will be created in-place.
     * @return detection Tensor, empty if there were no detection Tensor objects added to this AudioEvent when
     * this method was called
     */
    Tensor detection() {
        if (!_detection) {
            add_tensor("detection");
        }
        return _detection ? *_detection : nullptr;
    }

    /**
     * @brief Get label_id from detection Tensor, last added to this AudioEvent
     * @return last added detection Tensor label_id if exists, otherwise 0
     */
    int label_id() const {
        return _detection ? _detection->label_id() : 0;
    }

    /**
     * @brief Construct AudioEvent instance from GstGVAAudioEventMeta. After this, AudioEvent will
     * obtain all tensors (detection & inference results) from GstGVAAudioEventMeta
     * @param meta GstGVAAudioEventMeta containing AudioEVent information and tensors
     */
    AudioEvent(GstGVAAudioEventMeta *meta) : _gst_meta(meta), _detection(nullptr) {
        if (not _gst_meta)
            throw std::invalid_argument("GVA::AudioEvent: meta is nullptr");

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
     * @brief Set AudioEvent label
     * @param label Label to set
     */
    void set_label(std::string label) {
        _gst_meta->event_type = g_quark_from_string(label.c_str());
    }

    /**
     * @brief Internal function, don't use or use with caution.
     * @return pointer to underlying GstGVAAudioEventMeta
     */
    GstGVAAudioEventMeta *_meta() const {
        return _gst_meta;
    }

  protected:
    /**
     * @brief GstGVAAudioEventMeta containing fields filled with detection result (produced by gvaaudiodetect element
     * in Gstreamer pipeline) and all the additional tensors, describing detection
     */
    GstGVAAudioEventMeta *_gst_meta;
    /**
     * @brief vector of Tensor objects added to this AudioEvent (describing detection & inference results),
     * obtained from GstGVAAudioEventMeta
     */
    std::vector<Tensor> _tensors;
    /**
     * @brief last added detection Tensor instance, defined as Tensor with name set to "detection"
     */
    Tensor *_detection;
};

} // namespace GVA
