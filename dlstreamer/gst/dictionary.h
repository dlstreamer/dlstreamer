/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "buffer.h"
#include "dlstreamer/dictionary.h"
#include "dlstreamer/utils.h"
#include <gst/gst.h>

namespace dlstreamer {

class GSTDictionary : public Dictionary {
    friend class GSTBuffer;

  public:
    GSTDictionary(GstStructure *structure) : _structure(structure) {
    }

    std::string name() const override {
        return gst_structure_get_name(_structure);
    }
    virtual std::optional<Any> try_get(std::string const &key) const noexcept override {
        const GValue *gval = gst_structure_get_value(_structure, key.data());
        if (!gval)
            return {};
        if (G_VALUE_TYPE(gval) == GST_TYPE_ARRAY) {
            GValueArray *arr = nullptr;
            gst_structure_get_array(_structure, key.data(), &arr);
            if (!arr)
                return {};
            std::string ret;
            for (guint i = 0; i < arr->n_values; ++i) {
                if (i)
                    ret += ","; // TODO: is ',' safe enough separation symbol?
                ret += any_to_string(*g_value_to_dls_any(g_value_array_get_nth(arr, i)));
            }
            g_value_array_free(arr);
            return ret;
        }
        auto opt_val = g_value_to_dls_any(gval);
        if (!opt_val)
            return {};
        return *opt_val;
    }
    void set(std::string const &key, Any value) override {
        if (AnyHoldsType<int>(value)) {
            gst_structure_set(_structure, key.c_str(), G_TYPE_INT, AnyCast<int>(value), NULL);
        } else if (AnyHoldsType<double>(value)) {
            gst_structure_set(_structure, key.c_str(), G_TYPE_DOUBLE, AnyCast<double>(value), NULL);
        } else if (AnyHoldsType<bool>(value)) {
            gst_structure_set(_structure, key.c_str(), G_TYPE_BOOLEAN, AnyCast<bool>(value), NULL);
        } else if (AnyHoldsType<std::string>(value)) {
            gst_structure_set(_structure, key.c_str(), G_TYPE_STRING, AnyCast<std::string>(value).c_str(), NULL);
        } else if (AnyHoldsType<intptr_t>(value)) {
            gst_structure_set(_structure, key.c_str(), G_TYPE_POINTER, AnyCast<intptr_t>(value), NULL);
        } else {
            throw std::runtime_error("Unsupported data type");
        }
    }
    std::vector<std::string> keys() const override {
        std::vector<std::string> ret;
        for (gint i = 0; i < gst_structure_n_fields(_structure); i++) {
            ret.push_back(gst_structure_nth_field_name(_structure, i));
        }
        return ret;
    }
    void set_name(std::string const &name) {
        gst_structure_set_name(_structure, name.c_str());
    }

  protected:
    GstStructure *_structure;

    static std::optional<Any> g_value_to_dls_any(const GValue *gval) noexcept {
        switch (G_VALUE_TYPE(gval)) {
        case G_TYPE_INT:
            return g_value_get_int(gval);
        case G_TYPE_DOUBLE:
            return g_value_get_double(gval);
        case G_TYPE_BOOLEAN:
            return static_cast<bool>(g_value_get_boolean(gval));
        case G_TYPE_STRING:
            return std::string(g_value_get_string(gval));
        case G_TYPE_POINTER:
            return reinterpret_cast<intptr_t>(g_value_get_pointer(gval));
        default:
            return {};
        }
    }
};

} // namespace dlstreamer
