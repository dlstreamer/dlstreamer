/*******************************************************************************
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/frame.h"
#include "dlstreamer/gst/utils.h"
#include "dlstreamer/image_metadata.h"
#include <cmath>
#include <gst/gst.h>

namespace dlstreamer {

class GSTDictionary : public Dictionary {
    friend class GSTMetadata;
    friend class GSTROIMetadata;

  public:
    GSTDictionary(GstStructure *structure) : _structure(structure) {
    }

    std::string name() const override {
        return gst_structure_get_name(_structure);
    }

    virtual std::optional<Any> try_get(std::string_view key) const noexcept override {
        return gvalue_to_any(gst_structure_get_value(_structure, key.data()));
    }

    std::pair<const void *, size_t> try_get_array(std::string_view key) const noexcept override {
        const GValue *gval = gst_structure_get_value(_structure, key.data());
        if (!gval)
            return {nullptr, 0};
        GVariant *gvariant = g_value_get_variant(gval);
        gsize size = 0;
        const void *data = g_variant_get_fixed_array(gvariant, &size, 1);
        return {data, size};
    }

    void set(std::string_view key, Any value) override {
        GValue gvalue = G_VALUE_INIT;
        any_to_gvalue(value, &gvalue);
        gst_structure_set_value(_structure, key.data(), &gvalue);
    }

    void set_array(std::string_view key, const void *data, size_t nbytes) override {
        GVariant *gvariant_array = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, data, nbytes, 1);
        if (!data || !gvariant_array)
            throw std::runtime_error("Error creating g_variant_fixed_array");
        gst_structure_set(_structure, key.data(), G_TYPE_VARIANT, gvariant_array, NULL);
    }

    std::vector<std::string> keys() const override {
        std::vector<std::string> ret;
        for (gint i = 0; i < gst_structure_n_fields(_structure); i++) {
            ret.push_back(gst_structure_nth_field_name(_structure, i));
        }
        return ret;
    }

    void set_name(std::string const &name) override {
        if (!name.empty())
            gst_structure_set_name(_structure, name.c_str());
    }

  protected:
    GstStructure *_structure;
};

/////////////////////////////////////////////////////////////////////////////////////
// GSTROIDictionary

class GSTROIDictionary : public Dictionary {
  public:
    using key = DetectionMetadata::key;

    GSTROIDictionary(GstVideoRegionOfInterestMeta *roi, int width, int height, GstStructure *structure)
        : _roi(roi), _width(width), _height(height) {
        DLS_CHECK(width && height)
        DLS_CHECK(structure)
        _struct_dict = std::make_shared<GSTDictionary>(structure);
    }

    std::string name() const override {
        return DetectionMetadata::name;
    }

    virtual std::vector<std::string> keys() const override {
        using inferkey = InferenceResultMetadata::key;
        return {
            "x_min",
            "y_min",
            "x_max",
            "y_max",
            "confidence",
            "id",
            "parent_id",
            "label_id",
            "label",
            // keys from InferenceResultMetadata:
            inferkey::model_name,
            inferkey::layer_name,
            inferkey::format,
        };
    }

    std::optional<Any> try_get(std::string_view key) const noexcept override {
        if (key == key::id)
            return _roi->id;
        if (key == key::parent_id)
            return _roi->parent_id;
        if (key == key::label)
            return _roi->roi_type ? g_quark_to_string(_roi->roi_type) : std::string();
        return _struct_dict->try_get(key);
    }

    void set(std::string_view key, Any value) override {
        using inferkey = InferenceResultMetadata::key;
        if (key == key::x_min) {
            _roi->x = std::lround(any_cast<double>(value) * _width);
            _struct_dict->set(key, value);
        } else if (key == key::y_min) {
            _roi->y = std::lround(any_cast<double>(value) * _height);
            _struct_dict->set(key, value);
        } else if (key == key::x_max) {
            _roi->w = std::lround(any_cast<double>(value) * _width) - _roi->x;
            _struct_dict->set(key, value);
        } else if (key == key::y_max) {
            _roi->h = std::lround(any_cast<double>(value) * _height) - _roi->y;
            _struct_dict->set(key, value);
        } else if (key == key::id) {
            _roi->id = any_cast<int>(value);
        } else if (key == key::parent_id) {
            _roi->parent_id = any_cast<int>(value);
        } else if (key == key::label) {
            auto label = any_cast<std::string>(value);
            _roi->roi_type = g_quark_from_string(label.data());
        } else if (key == key::label_id) {
            _struct_dict->set(key, value);
        } else if (key == key::confidence) {
            _struct_dict->set(key, value);
        } else if (key == inferkey::model_name) {
            _struct_dict->set(key, value);
        } else if (key == inferkey::layer_name) {
            _struct_dict->set(key, value);
        } else if (key == inferkey::format) {
            _struct_dict->set(key, value);
        } else {
            throw std::runtime_error("Unsupported key: " + std::string(key));
        }
    }

    std::pair<const void *, size_t> try_get_array(std::string_view key) const noexcept override {
        return _struct_dict->try_get_array(key);
    }

    void set_array(std::string_view key, const void *data, size_t nbytes) override {
        _struct_dict->set_array(key, data, nbytes);
    }

    virtual void set_name(std::string const &) override {
        throw std::runtime_error("Unsupported");
    }

  private:
    GstVideoRegionOfInterestMeta *_roi;
    DictionaryPtr _struct_dict;
    double _width;
    double _height;
};

} // namespace dlstreamer
