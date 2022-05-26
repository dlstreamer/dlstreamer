/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/dictionary.h"

namespace dlstreamer {

class DetectionMetadata : DictionaryProxy {
  public:
    static constexpr auto name = "detection";
    struct key {
        static constexpr auto x_min = "x_min";           // double
        static constexpr auto y_min = "y_min";           // double
        static constexpr auto x_max = "x_max";           // double
        static constexpr auto y_max = "y_max";           // double
        static constexpr auto confidence = "confidence"; // double
        static constexpr auto label_id = "label_id";     // int
        static constexpr auto label = "label";           // std::string
    };

    DetectionMetadata() : DictionaryProxy(std::make_shared<STDDictionary>(name)) {
    }
    DetectionMetadata(DictionaryPtr dict) : DictionaryProxy(dict) {
    }
    inline double x_min() const {
        return _dict->get<double>(key::x_min);
    }
    inline double y_min() const {
        return _dict->get<double>(key::y_min);
    }
    inline double x_max() const {
        return _dict->get<double>(key::x_max);
    }
    inline double y_max() const {
        return _dict->get<double>(key::y_max);
    }
    inline double confidence() const {
        return _dict->get<double>(key::confidence);
    }
    inline int label_id() const {
        return _dict->get<int>(key::label_id);
    }
    inline std::string label() const {
        return _dict->get<std::string>(key::label);
    }
    void init(double x_min, double y_min, double x_max, double y_max, double confidence = 0.0, int label_id = 0,
              std::string label = std::string()) {
        _dict->set(key::x_min, x_min);
        _dict->set(key::y_min, y_min);
        _dict->set(key::x_max, x_max);
        _dict->set(key::y_max, y_max);
        _dict->set(key::confidence, confidence);
        _dict->set(key::label_id, label_id);
        if (!label.empty())
            _dict->set(key::label, label);
    }
};

class SourceIdentifierMetadata : public DictionaryProxy {
  public:
    static constexpr auto name = "SourceIdentifierMetadata";
    struct key {
        static constexpr auto batch_index = "batch_index"; // int
        static constexpr auto pts = "pts";                 // intptr_t (nanoseconds)
        static constexpr auto stream_id = "stream_id";     // intptr_t
        static constexpr auto roi_id = "roi_id";           // int
        static constexpr auto object_id = "object_id";     // int
    };
    using DictionaryProxy::DictionaryProxy;

    static std::shared_ptr<SourceIdentifierMetadata> try_cast(DictionaryPtr dict) {
        if (!dict || dict->name() != name)
            return nullptr;
        return std::make_shared<SourceIdentifierMetadata>(dict);
    }

    inline int batch_index() const {
        return _dict->get<int>(key::batch_index);
    }
    inline int64_t pts() const {
        return _dict->get<intptr_t>(key::pts);
    }
    inline intptr_t stream_id() const {
        return _dict->get<intptr_t>(key::stream_id);
    }
    inline int roi_id() const {
        return _dict->get<int>(key::roi_id);
    }
    inline int object_id() const {
        return _dict->get<int>(key::object_id);
    }

    void init(int batch_index, int64_t pts, intptr_t stream_id, int roi_id, int object_id = 0) {
        _dict->set(key::batch_index, batch_index);
        _dict->set(key::pts, static_cast<intptr_t>(pts));
        _dict->set(key::stream_id, stream_id);
        _dict->set(key::roi_id, roi_id);
        _dict->set(key::object_id, object_id);
    }
};

} // namespace dlstreamer
