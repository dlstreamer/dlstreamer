/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvaclassifybin.hpp"
#include <stdexcept>

GST_DEBUG_CATEGORY(gva_classify_bin_debug_category);
#define GST_CAT_DEFAULT gva_classify_bin_debug_category

namespace {
constexpr guint MIN_RECLASSIFY_INTERVAL = 0;
constexpr guint MAX_RECLASSIFY_INTERVAL = std::numeric_limits<guint>::max();
constexpr guint DEFAULT_RECLASSIFY_INTERVAL = 1;
} // namespace

/* Properties */
enum { PROP_0, PROP_RECLASSIFY_INTERVAL };

class GvaClassifyBinPrivate {
  public:
    static GvaClassifyBinPrivate *unpack(gpointer base) {
        g_assert(GVA_INFERENCE_BIN(base)->impl);
        return GVA_CLASSIFY_BIN(base)->impl;
    }

    GvaClassifyBinPrivate(GvaInferenceBin *base) : _base(base) {
    }

    void get_property(guint prop_id, GValue *value, GParamSpec *pspec);
    void set_property(guint prop_id, const GValue *value, GParamSpec *pspec);

    bool init_preprocessing(PreProcessBackend linkage, std::list<GstElement *> &link_order);

    GstElement *create_legacy_element() {
        if (!_legacy_inference)
            _legacy_inference = gst_element_factory_make("gvaclassify_legacy", nullptr);
        return _legacy_inference;
    }

    GstStateChangeReturn change_state(GstStateChange transition);

  private:
    GvaInferenceBin *_base = nullptr;

    GstElement *_legacy_inference = nullptr;

    uint32_t _reclassify_interval = 1;
};

#define gva_classify_bin_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE(GvaClassifyBin, gva_classify_bin, GST_TYPE_GVA_INFERENCE_BIN);

bool GvaClassifyBinPrivate::init_preprocessing(PreProcessBackend linkage, std::list<GstElement *> &link_order) {
    if (!GVA_INFERENCE_BIN_CLASS(parent_class)->init_preprocessing(GST_BIN(_base), linkage, link_order))
        return false;

    auto roi_split_iter = std::find_if(link_order.begin(), link_order.end(), [](const auto &element) {
        std::string name = gst_element_get_name(element);
        return name.rfind("roi_split", 0) == 0;
    });

    // Reclassify interval equals to 1 meaning we won't use history.
    if (_reclassify_interval != 1) {
        auto history =
            create_element("gvahistory", {{"type", "meta"}, {"interval", std::to_string(_reclassify_interval)}});
        // We should put history after roisplit
        if (roi_split_iter != link_order.end())
            link_order.insert(std::next(roi_split_iter), history);
        else
            link_order.push_front(history);
    }

    return true;
}

void GvaClassifyBinPrivate::get_property(guint prop_id, GValue *value, GParamSpec *pspec) {
    switch (prop_id) {
    case PROP_RECLASSIFY_INTERVAL:
        g_value_set_uint(value, _reclassify_interval);
        break;
    default:
        G_OBJECT_CLASS(parent_class)->get_property(G_OBJECT(_base), prop_id, value, pspec);
        break;
    }
}

void GvaClassifyBinPrivate::set_property(guint prop_id, const GValue *value, GParamSpec *pspec) {
    switch (prop_id) {
    case PROP_RECLASSIFY_INTERVAL:
        _reclassify_interval = g_value_get_uint(value);
        break;
    default:
        G_OBJECT_CLASS(parent_class)->set_property(G_OBJECT(_base), prop_id, value, pspec);
        break;
    }
}

GstStateChangeReturn GvaClassifyBinPrivate::change_state(GstStateChange transition) {
    auto ret = GST_ELEMENT_CLASS(parent_class)->change_state(GST_ELEMENT(_base), transition);
    if (ret == GST_STATE_CHANGE_SUCCESS && transition == GST_STATE_CHANGE_NULL_TO_READY && _legacy_inference) {
        g_object_set(_legacy_inference, "reclassify-interval", _reclassify_interval, nullptr);
    }
    return ret;
}

static void gva_classify_bin_init(GvaClassifyBin *self) {
    // Initialize of private data
    auto *priv_memory = gva_classify_bin_get_instance_private(self);
    self->impl = new (priv_memory) GvaClassifyBinPrivate(&self->base);

    // Override default values
    self->base.set_converter_type(post_processing::ConverterType::TO_TENSOR);
    gst_util_set_object_arg(G_OBJECT(self), "inference-region", "roi-list");
}

static void gva_classify_bin_finalize(GObject *object) {
    GvaClassifyBin *self = GVA_CLASSIFY_BIN(object);
    if (self->impl) {
        // Manually invoke object destruction since it was created via placement-new.
        self->impl->~GvaClassifyBinPrivate();
        self->impl = nullptr;
    }

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gva_classify_bin_class_init(GvaClassifyBinClass *klass) {
    GST_DEBUG_CATEGORY_INIT(gva_classify_bin_debug_category, "gvaclassify", 0, "Debug category of gvaclassify");

    auto gobject_class = G_OBJECT_CLASS(klass);
    auto element_class = GST_ELEMENT_CLASS(klass);
    auto inference_class = GVA_INFERENCE_BIN_CLASS(klass);

    gobject_class->set_property = [](GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
        GvaClassifyBinPrivate::unpack(object)->set_property(property_id, value, pspec);
    };
    gobject_class->get_property = [](GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
        GvaClassifyBinPrivate::unpack(object)->get_property(property_id, value, pspec);
    };
    gobject_class->finalize = gva_classify_bin_finalize;

    element_class->change_state = [](GstElement *element, GstStateChange transition) {
        return GvaClassifyBinPrivate::unpack(element)->change_state(transition);
    };

    inference_class->init_preprocessing = [](GstBin *bin, PreProcessBackend linkage,
                                             std::list<GstElement *> &link_order) {
        return GvaClassifyBinPrivate::unpack(bin)->init_preprocessing(linkage, link_order);
    };
    inference_class->create_legacy_element = [](GstBin *bin) {
        return GvaClassifyBinPrivate::unpack(bin)->create_legacy_element();
    };

    constexpr auto prm_flags = static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);
    g_object_class_install_property(
        gobject_class, PROP_RECLASSIFY_INTERVAL,
        g_param_spec_uint(
            "reclassify-interval", "Reclassify Interval",
            "Determines how often to reclassify tracked objects. Only valid when used in conjunction with gvatrack.\n"
            "The following values are acceptable:\n"
            "- 0 - Do not reclassify tracked objects\n"
            "- 1 - Always reclassify tracked objects\n"
            "- N (>=2) - Tracked objects will be reclassified every N frames. Note the inference-interval is applied "
            "before determining if an object is to be reclassified (i.e. classification only occurs at a multiple of "
            "the inference interval)",
            MIN_RECLASSIFY_INTERVAL, MAX_RECLASSIFY_INTERVAL, DEFAULT_RECLASSIFY_INTERVAL, prm_flags));

    gst_element_class_set_metadata(element_class, GVA_CLASSIFY_BIN_NAME, "video", GVA_CLASSIFY_BIN_DESCRIPTION,
                                   "Intel Corporation");
}
