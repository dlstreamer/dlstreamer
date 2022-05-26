/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvafilter.hpp"
#include "filters/meta_filter.hpp"

#include <capabilities/tensor_caps.hpp>
#include <meta/gva_buffer_flags.hpp>
#include <tensor_layer_desc.hpp>

#include <gst/gstevent.h>

#include <stdexcept>

namespace {

constexpr auto DEFAULT_TYPE = FilterType::FILTER_TYPE_META;

// Enum value names
constexpr auto UNKNOWN_VALUE_NAME = "unknown";
constexpr auto TYPE_META_NAME = "meta";

GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS("ANY"));
GstStaticPadTemplate sinktemplate =
    GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS("ANY"));

std::string type_to_string(FilterType type) {
    switch (type) {
    case FILTER_TYPE_META:
        return TYPE_META_NAME;
    default:
        return UNKNOWN_VALUE_NAME;
    }
}

enum { PROP_0, PROP_TYPE, PROP_OBJECT_CLASS };

std::unique_ptr<IFilter> create_filter(GvaFilter *gvafilter) {
    g_assert(gvafilter && "GvaFilter instance is null");

    try {
        switch (gvafilter->props.type) {
        case FilterType::FILTER_TYPE_META:
            return std::unique_ptr<MetaFilter>(new MetaFilter(gvafilter->props.object_class_filter));
        default:
            throw std::runtime_error("Unsupported filter type");
        }
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(gvafilter, "Error while creating filter instance: %s", Utils::createNestedErrorMsg(e).c_str());
    }

    return nullptr;
}

} // namespace

GST_DEBUG_CATEGORY(gva_filter_debug_category);
#define GST_CAT_DEFAULT gva_filter_debug_category

G_DEFINE_TYPE(GvaFilter, gva_filter, GST_TYPE_BASE_TRANSFORM);

#define GST_TYPE_GVA_FILTER_TYPE (gva_filter_type_get_type())
static GType gva_filter_type_get_type(void) {
    static const GEnumValue types[] = {{FilterType::FILTER_TYPE_META, "Meta", TYPE_META_NAME}, {0, NULL, NULL}};
    static GType gva_filter_type = g_enum_register_static("GvaFilterMode", types);
    return gva_filter_type;
}

static void gva_filter_init(GvaFilter *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // Initialize C++ structure with placement new
    new (&self->props) GvaFilter::_Props();
}

static void gva_filter_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GvaFilter *self = GVA_FILTER(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (prop_id) {
    case PROP_TYPE:
        self->props.type = static_cast<FilterType>(g_value_get_enum(value));
        break;
    case PROP_OBJECT_CLASS:
        self->props.object_class_filter = g_value_get_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gva_filter_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GvaFilter *self = GVA_FILTER(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (prop_id) {
    case PROP_TYPE:
        g_value_set_enum(value, self->props.type);
        break;
    case PROP_OBJECT_CLASS:
        g_value_set_string(value, self->props.object_class_filter.c_str());
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gva_filter_finalize(GObject *object) {
    GvaFilter *self = GVA_FILTER(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // Destroy C++ structure manually
    self->props.~_Props();

    G_OBJECT_CLASS(gva_filter_parent_class)->finalize(object);
}

static gboolean gva_filter_start(GstBaseTransform *base) {
    GvaFilter *self = GVA_FILTER(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    GST_INFO_OBJECT(self, "%s parameters:\n -- Type: %s\n", GST_ELEMENT_NAME(GST_ELEMENT_CAST(self)),
                    type_to_string(self->props.type).c_str());

    self->props.filter = create_filter(self);
    if (!self->props.filter) {
        GST_ERROR_OBJECT(self, "Failed to create filter instance");
        return false;
    }

    return true;
}

static GstFlowReturn gva_filter_transform_ip(GstBaseTransform *base, GstBuffer *buf) {
    GvaFilter *self = GVA_FILTER(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    try {
        self->props.filter->invoke(buf);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Error during filtering: %s", Utils::createNestedErrorMsg(e).c_str());
        return GST_FLOW_ERROR;
    }

    return GST_FLOW_OK;
}

static void gva_filter_class_init(GvaFilterClass *klass) {

    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = gva_filter_set_property;
    gobject_class->get_property = gva_filter_get_property;
    gobject_class->finalize = gva_filter_finalize;

    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->start = gva_filter_start;
    base_transform_class->transform_ip = gva_filter_transform_ip;

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, GVA_FILTER_NAME, "application", GVA_FILTER_DESCRIPTION,
                                          "Intel Corporation");

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sinktemplate));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&srctemplate));

    constexpr auto prm_flags = static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);
    g_object_class_install_property(gobject_class, PROP_TYPE,
                                    g_param_spec_enum("type", "Filter type", "Type defining which filter to use",
                                                      GST_TYPE_GVA_FILTER_TYPE, DEFAULT_TYPE, prm_flags));
    g_object_class_install_property(
        gobject_class, PROP_OBJECT_CLASS,
        g_param_spec_string("object-class", "ObjectClass",
                            "[type=meta] Filter for Region of Interest class label on this element input", "",
                            prm_flags));
}
