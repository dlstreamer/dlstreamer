/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvadetectbin.hpp"

#include <gva_caps.h>
#include <post_processor/post_proc_common.h>

#include <list>

GST_DEBUG_CATEGORY(gva_detect_bin_debug_category);
#define GST_CAT_DEFAULT gva_detect_bin_debug_category

namespace {
constexpr auto MAX_THRESHOLD = 1.f;
constexpr auto MIN_THRESHOLD = 0.f;
constexpr auto DEFAULT_THRESHOLD = 0.5f;
} // namespace

/* Properties */
enum { PROP_0, PROP_THRESHOLD };

class GvaDetectBinPrivate {
  public:
    static GvaDetectBinPrivate *unpack(gpointer base) {
        g_assert(GVA_INFERENCE_BIN(base)->impl);
        return GVA_DETECT_BIN(base)->impl;
    }

    GvaDetectBinPrivate(GvaInferenceBin *base) : _base(base) {
    }

    GstElement *create_legacy_element() {
        if (!_legacy_inference)
            _legacy_inference = gst_element_factory_make("gvadetect_legacy", nullptr);
        return _legacy_inference;
    }

    GstElement *init_postprocessing();

    void get_property(guint property_id, GValue *value, GParamSpec *pspec);
    void set_property(guint property_id, const GValue *value, GParamSpec *pspec);

    GstStateChangeReturn change_state(GstStateChange transition);

  private:
    GvaInferenceBin *_base = nullptr;

    GstElement *_legacy_inference = nullptr;

    float _threshold = DEFAULT_THRESHOLD;
};

#define gva_detect_bin_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE(GvaDetectBin, gva_detect_bin, GST_TYPE_GVA_INFERENCE_BIN)

GstElement *GvaDetectBinPrivate::init_postprocessing() {
    GstElement *postproc = GVA_INFERENCE_BIN_CLASS(parent_class)->init_postprocessing(GST_BIN(_base));
    if (!postproc)
        return nullptr;
    g_object_set(G_OBJECT(postproc), "threshold", _threshold, nullptr);
    return postproc;
}

void GvaDetectBinPrivate::get_property(guint property_id, GValue *value, GParamSpec *pspec) {
    switch (property_id) {
    case PROP_THRESHOLD:
        g_value_set_float(value, _threshold);
        break;
    default:
        G_OBJECT_CLASS(parent_class)->get_property(G_OBJECT(_base), property_id, value, pspec);
        break;
    }
}

void GvaDetectBinPrivate::set_property(guint property_id, const GValue *value, GParamSpec *pspec) {
    switch (property_id) {
    case PROP_THRESHOLD:
        _threshold = g_value_get_float(value);
        break;
    default:
        G_OBJECT_CLASS(parent_class)->set_property(G_OBJECT(_base), property_id, value, pspec);
        break;
    }
}

GstStateChangeReturn GvaDetectBinPrivate::change_state(GstStateChange transition) {
    auto ret = GST_ELEMENT_CLASS(parent_class)->change_state(GST_ELEMENT(_base), transition);
    if (ret == GST_STATE_CHANGE_SUCCESS && transition == GST_STATE_CHANGE_NULL_TO_READY) {
        if (_legacy_inference)
            g_object_set(_legacy_inference, "threshold", _threshold, nullptr);
    }
    return ret;
}

static void gva_detect_bin_init(GvaDetectBin *self) {
    // Initialize of private data
    auto *priv_memory = gva_detect_bin_get_instance_private(self);
    self->impl = new (priv_memory) GvaDetectBinPrivate(&self->base);

    self->base.set_converter_type(post_processing::ConverterType::TO_ROI);
}

static void gva_detect_bin_finalize(GObject *object) {
    auto self = GVA_DETECT_BIN(object);
    if (self->impl) {
        // Manually invoke object destruction since it was created via placement-new.
        self->impl->~GvaDetectBinPrivate();
        self->impl = nullptr;
    }

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gva_detect_bin_class_init(GvaDetectBinClass *klass) {
    GST_DEBUG_CATEGORY_INIT(gva_detect_bin_debug_category, "gvadetect", 0, "Debug category of gvadetect");

    auto inference_class = GVA_INFERENCE_BIN_CLASS(klass);
    inference_class->create_legacy_element = [](GstBin *bin) {
        return GvaDetectBinPrivate::unpack(bin)->create_legacy_element();
    };

    inference_class->init_postprocessing = [](GstBin *bin) {
        return GvaDetectBinPrivate::unpack(bin)->init_postprocessing();
    };

    auto element_class = GST_ELEMENT_CLASS(klass);
    element_class->change_state = [](GstElement *element, GstStateChange transition) {
        return GvaDetectBinPrivate::unpack(element)->change_state(transition);
    };

    gst_element_class_set_metadata(element_class, GVA_DETECT_BIN_NAME, "video", GVA_DETECT_BIN_DESCRIPTION,
                                   "Intel Corporation");

    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->get_property = [](GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
        GvaDetectBinPrivate::unpack(object)->get_property(property_id, value, pspec);
    };
    gobject_class->set_property = [](GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
        GvaDetectBinPrivate::unpack(object)->set_property(property_id, value, pspec);
    };
    gobject_class->finalize = gva_detect_bin_finalize;

    g_object_class_install_property(
        gobject_class, PROP_THRESHOLD,
        g_param_spec_float("threshold", "Threshold",
                           "Threshold for detection results. Only regions of interest "
                           "with confidence values above the threshold will be added to the frame",
                           MIN_THRESHOLD, MAX_THRESHOLD, DEFAULT_THRESHOLD,
                           static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));
}
