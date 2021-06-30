/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvafpscounter.h"

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>

#include "fpscounter_c.h"
#include "gva_caps.h"

#include "config.h"
#include "utils.h"
#include <regex.h>

/* enable usage with application/tensor caps */
#include <capabilities/capabilities.hpp>
#define GVA_MIXED_CAPS GVA_CAPS GVA_TENSOR_CAPS

#define ELEMENT_LONG_NAME "Frames Per Second counter"
#define ELEMENT_DESCRIPTION "Measures frames per second across multiple streams in a single process."

GST_DEBUG_CATEGORY_STATIC(gst_gva_fpscounter_debug_category);
#define GST_CAT_DEFAULT gst_gva_fpscounter_debug_category

enum { PROP_0, PROP_INTERVAL, PROP_STARTING_FRAME };

#define DEFAULT_INTERVAL "1"

#define DEFAULT_STARTING_FRAME 0
#define DEFAULT_MIN_STARTING_FRAME 0
#define DEFAULT_MAX_STARTING_FRAME UINT_MAX

/* prototypes */
static void gst_gva_fpscounter_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void gst_gva_fpscounter_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static gboolean gst_gva_fpscounter_check_interval_value(const GValue *value);
static gboolean gst_gva_fpscounter_start(GstBaseTransform *trans);
static void gst_gva_fpscounter_dispose(GObject *object);
static void gst_gva_fpscounter_finalize(GObject *object);
static void gst_gva_fpscounter_cleanup(GstGvaFpscounter *gva_fpscounter);

static GstFlowReturn gst_gva_fpscounter_transform_ip(GstBaseTransform *trans, GstBuffer *buf);
static gboolean gst_gva_fpscounter_sink_event(GstBaseTransform *trans, GstEvent *event);

/* class initialization */
static void gst_gva_fpscounter_init(GstGvaFpscounter *gva_fpscounter);

G_DEFINE_TYPE_WITH_CODE(GstGvaFpscounter, gst_gva_fpscounter, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_fpscounter_debug_category, "gvafpscounter", 0,
                                                "debug category for gvafpscounter element"));

static void gst_gva_fpscounter_class_init(GstGvaFpscounterClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    /* Setting up pads and setting metadata should be moved to
       base_class_init if you intend to subclass this class. */
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_MIXED_CAPS)));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(GVA_MIXED_CAPS)));

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");
    gobject_class->set_property = gst_gva_fpscounter_set_property;
    gobject_class->get_property = gst_gva_fpscounter_get_property;
    gobject_class->dispose = gst_gva_fpscounter_dispose;
    gobject_class->finalize = gst_gva_fpscounter_finalize;
    base_transform_class->start = GST_DEBUG_FUNCPTR(gst_gva_fpscounter_start);
    base_transform_class->transform = NULL;
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_gva_fpscounter_transform_ip);
    base_transform_class->sink_event = GST_DEBUG_FUNCPTR(gst_gva_fpscounter_sink_event);

    g_object_class_install_property(
        gobject_class, PROP_INTERVAL,
        g_param_spec_string("interval", "Interval", "The time interval in seconds for which the fps will be measured",
                            DEFAULT_INTERVAL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class, PROP_STARTING_FRAME,
        g_param_spec_uint("starting-frame", "Starting frame",
                          "Start collecting fps measurements after the specified number of frames have been "
                          "processed to remove the influence of initialization cost",
                          DEFAULT_MIN_STARTING_FRAME, DEFAULT_MAX_STARTING_FRAME, DEFAULT_STARTING_FRAME,
                          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void gst_gva_fpscounter_init(GstGvaFpscounter *gva_fpscounter) {
    GST_DEBUG_OBJECT(gva_fpscounter, "gva_fpscounter_init");

    gst_gva_fpscounter_cleanup(gva_fpscounter);

    gva_fpscounter->interval = g_strdup(DEFAULT_INTERVAL);
    gva_fpscounter->starting_frame = DEFAULT_STARTING_FRAME;
}

void gst_gva_fpscounter_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    GstGvaFpscounter *gvafpscounter = GST_GVA_FPSCOUNTER(object);

    GST_DEBUG_OBJECT(gvafpscounter, "get_property");
    switch (property_id) {
    case PROP_INTERVAL:
        g_value_set_string(value, gvafpscounter->interval);
        break;
    case PROP_STARTING_FRAME:
        g_value_set_uint(value, gvafpscounter->starting_frame);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static gboolean gst_gva_fpscounter_check_interval_value(const GValue *value) {
    /* check if interval property is valid */
    regex_t interval_reg;
    g_assert(regcomp(&interval_reg, "^[0-9]{1,9}(,[0-9]{1,9})*$", REG_EXTENDED) == 0);
    if (regexec(&interval_reg, g_value_get_string(value), 0, NULL, 0) != 0) {
        g_warning("gvafpscounter's interval property value is invalid."
                  " Positive integers must be used (may be comma separated). Default value will be set.");
        return FALSE;
    }

    return TRUE;
}

void gst_gva_fpscounter_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    GstGvaFpscounter *gvafpscounter = GST_GVA_FPSCOUNTER(object);

    GST_DEBUG_OBJECT(gvafpscounter, "set_property");
    switch (property_id) {
    case PROP_INTERVAL:
        g_free(gvafpscounter->interval);
        if (gst_gva_fpscounter_check_interval_value(value)) {
            gvafpscounter->interval = g_value_dup_string(value);
        } else {
            gvafpscounter->interval = g_strdup(DEFAULT_INTERVAL);
        }
        break;
    case PROP_STARTING_FRAME:
        gvafpscounter->starting_frame = g_value_get_uint(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void gst_gva_fpscounter_cleanup(GstGvaFpscounter *gva_fpscounter) {
    if (gva_fpscounter == NULL)
        return;

    GST_DEBUG_OBJECT(gva_fpscounter, "gst_gva_fpscounter_cleanup");

    if (gva_fpscounter->interval) {
        g_free(gva_fpscounter->interval);
        gva_fpscounter->interval = NULL;
    }
}

static gboolean gst_gva_fpscounter_start(GstBaseTransform *trans) {
    GstGvaFpscounter *gvafpscounter = GST_GVA_FPSCOUNTER(trans);

    GST_DEBUG_OBJECT(gvafpscounter, "start");
    fps_counter_create_average(gvafpscounter->starting_frame);
    fps_counter_create_iterative(gvafpscounter->interval);
    return TRUE;
}

gboolean gst_gva_fpscounter_sink_event(GstBaseTransform *trans, GstEvent *event) {
    UNUSED(trans);

    if (event->type == GST_EVENT_EOS) {
        fps_counter_eos();
    }

    return GST_BASE_TRANSFORM_CLASS(gst_gva_fpscounter_parent_class)->sink_event(trans, event);
}

void gst_gva_fpscounter_dispose(GObject *object) {
    GstGvaFpscounter *gvafpscounter = GST_GVA_FPSCOUNTER(object);

    GST_DEBUG_OBJECT(gvafpscounter, "dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gst_gva_fpscounter_parent_class)->dispose(object);
}

void gst_gva_fpscounter_finalize(GObject *object) {
    GstGvaFpscounter *gvafpscounter = GST_GVA_FPSCOUNTER(object);

    GST_DEBUG_OBJECT(gvafpscounter, "finalize");

    /* clean up object here */
    gst_gva_fpscounter_cleanup(gvafpscounter);

    G_OBJECT_CLASS(gst_gva_fpscounter_parent_class)->finalize(object);
}

static GstFlowReturn gst_gva_fpscounter_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
    GstGvaFpscounter *gvafpscounter = GST_GVA_FPSCOUNTER(trans);

    GST_DEBUG_OBJECT(gvafpscounter, "transform_ip");

    fps_counter_new_frame(buf, GST_ELEMENT_NAME(GST_ELEMENT(trans)));

    if (!gst_pad_is_linked(GST_BASE_TRANSFORM_SRC_PAD(trans))) {
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }

    return GST_FLOW_OK;
}
