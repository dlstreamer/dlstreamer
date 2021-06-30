/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvatensortometa.hpp"
#include "post_processor.hpp"

#include <capabilities/capabilities.hpp>
#include <frame_data.hpp>
#include <utils.h>

GST_DEBUG_CATEGORY(gst_gva_tensor_to_meta_debug_category);

G_DEFINE_TYPE(GstGvaTensorToMeta, gst_gva_tensor_to_meta, GST_TYPE_BASE_TRANSFORM);

enum { PROP_0, PROP_MODEL_PROC };

static void gst_gva_tensor_to_meta_init(GstGvaTensorToMeta *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // Initialize C++ structure with new
    new (&self->props) GstGvaTensorToMeta::_Props();
}

gboolean check_gva_tensor_to_meta_stopped(GstGvaTensorToMeta *self) {
    GstState state;
    gboolean is_stopped;

    GST_OBJECT_LOCK(self);
    state = GST_STATE(self);
    is_stopped = state == GST_STATE_READY || state == GST_STATE_NULL;
    GST_OBJECT_UNLOCK(self);
    return is_stopped;
}

void gst_gva_tensor_to_meta_set_model_proc(GstGvaTensorToMeta *self, const gchar *model_proc_path) {
    if (check_gva_tensor_to_meta_stopped(self)) {
        self->props.model_proc = g_strdup(model_proc_path);
        GST_INFO_OBJECT(self, "model-proc: %s", self->props.model_proc.c_str());
    } else {
        GST_ELEMENT_WARNING(self, RESOURCE, SETTINGS, ("'model-proc' can't be changed"),
                            ("You cannot change 'model-proc' property on tensor_to_meta when a file is open"));
    }
}

static void gst_gva_tensor_to_meta_set_property(GObject *object, guint prop_id, const GValue *value,
                                                GParamSpec *pspec) {
    GstGvaTensorToMeta *self = GST_GVA_TENSOR_TO_META(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (prop_id) {
    case PROP_MODEL_PROC:
        gst_gva_tensor_to_meta_set_model_proc(self, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_gva_tensor_to_meta_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstGvaTensorToMeta *self = GST_GVA_TENSOR_TO_META(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (prop_id) {
    case PROP_MODEL_PROC:
        g_value_set_string(value, self->props.model_proc.c_str());
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_gva_tensor_to_meta_dispose(GObject *object) {
    GstGvaTensorToMeta *self = GST_GVA_TENSOR_TO_META(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);
    self->props.postproc.reset();

    G_OBJECT_CLASS(gst_gva_tensor_to_meta_parent_class)->dispose(object);
}

static void gst_gva_tensor_to_meta_finalize(GObject *object) {
    GstGvaTensorToMeta *self = GST_GVA_TENSOR_TO_META(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // Destroy C++ structure manually
    self->props.~_Props();

    G_OBJECT_CLASS(gst_gva_tensor_to_meta_parent_class)->finalize(object);
}

static gboolean gst_gva_tensor_to_meta_set_caps(GstBaseTransform *base, GstCaps *incaps, GstCaps *outcaps) {
    GstGvaTensorToMeta *self = GST_GVA_TENSOR_TO_META(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    try {
        self->props.tensor_caps = TensorCaps(incaps);
        self->props.postproc.reset(new PostProcessor(self->props.tensor_caps, self->props.model_proc));
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Failed to parse tensor capabilities: %s", Utils::createNestedErrorMsg(e).c_str());
        return false;
    }

    UNUSED(incaps);
    UNUSED(outcaps);
    return true;
}

static gboolean gst_gva_tensor_to_meta_sink_event(GstBaseTransform *base, GstEvent *event) {
    GstGvaTensorToMeta *self = GST_GVA_TENSOR_TO_META(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return GST_BASE_TRANSFORM_CLASS(gst_gva_tensor_to_meta_parent_class)->sink_event(base, event);
}

static gboolean gst_gva_tensor_to_meta_start(GstBaseTransform *base) {
    GstGvaTensorToMeta *self = GST_GVA_TENSOR_TO_META(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return true;
}

static gboolean gst_gva_tensor_to_meta_stop(GstBaseTransform *base) {
    GstGvaTensorToMeta *self = GST_GVA_TENSOR_TO_META(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return true;
}

static GstFlowReturn gst_gva_tensor_to_meta_transform_ip(GstBaseTransform *base, GstBuffer *buffer) {
    GstGvaTensorToMeta *self = GST_GVA_TENSOR_TO_META(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    self->props.postproc->process(buffer, self->props.tensor_caps);

    return GST_FLOW_OK;
}

static void gst_gva_tensor_to_meta_class_init(GstGvaTensorToMetaClass *klass) {

    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = gst_gva_tensor_to_meta_set_property;
    gobject_class->get_property = gst_gva_tensor_to_meta_get_property;
    gobject_class->dispose = gst_gva_tensor_to_meta_dispose;
    gobject_class->finalize = gst_gva_tensor_to_meta_finalize;

    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->set_caps = gst_gva_tensor_to_meta_set_caps;
    base_transform_class->sink_event = gst_gva_tensor_to_meta_sink_event;
    base_transform_class->start = gst_gva_tensor_to_meta_start;
    base_transform_class->stop = gst_gva_tensor_to_meta_stop;
    base_transform_class->transform_ip = gst_gva_tensor_to_meta_transform_ip;

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, GVA_TENSOR_TO_META_NAME, "application",
                                          GVA_TENSOR_TO_META_DESCRIPTION, "Intel Corporation");

    g_object_class_install_property(gobject_class, PROP_MODEL_PROC,
                                    g_param_spec_string("model-proc", "Model proc", "Path to model proc file", NULL,
                                                        (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_TENSOR_CAPS)));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(GVA_TENSOR_CAPS)));
}
