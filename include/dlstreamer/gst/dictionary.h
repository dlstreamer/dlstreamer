/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/frame.h"
#include "dlstreamer/gst/utils.h"
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

} // namespace dlstreamer
