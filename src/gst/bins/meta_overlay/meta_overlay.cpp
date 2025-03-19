/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "meta_overlay.h"
#include "dlstreamer/gst/utils.h"
#include "elem_names.h"

#include <string>

GST_DEBUG_CATEGORY_STATIC(meta_overlay_bin_debug_category);
#define GST_CAT_DEFAULT meta_overlay_bin_debug_category

G_DEFINE_TYPE_WITH_CODE(GstMetaOverlayBin, meta_overlay_bin, GST_TYPE_PROCESSBIN,
                        GST_DEBUG_CATEGORY_INIT(meta_overlay_bin_debug_category, "meta_overlay", 0,
                                                "Debug category for meta_overlay element"));

using namespace dlstreamer;

namespace {
typedef enum { CPU = 1, GPU = 2 } MetaOverlayDevice;

constexpr auto DEFAULT_DEVICE = CPU;

inline bool is_vaapipostproc_available() {
    GstElement *element = gst_element_factory_make(elem::vaapipostproc, "postproc");
    bool res = (element) ? true : false;
    gst_object_unref(GST_OBJECT(element));
    return res;
}
inline bool is_vapostproc_available() {
    GstElement *element = gst_element_factory_make(elem::vapostproc, "postproc");
    bool res = (element) ? true : false;
    gst_object_unref(GST_OBJECT(element));
    return res;
}
} // namespace

/* Properties */
enum { PROP_0, PROP_DEVICE };

#define GST_TYPE_META_OVERLAY_DEVICE (get_meta_overlay_device())
static GType get_meta_overlay_device(void) {
    static const GEnumValue device[] = {
        {CPU, "CPU device on system memory", "CPU"}, {GPU, "GPU device on video memory", "GPU"}, {0, NULL, NULL}};

    return g_enum_register_static("MetaOverlayDevice", device);
}

class PipeBuilder {
  public:
    virtual std::string get_preproc() const = 0;
    virtual std::string get_process() const = 0;
    virtual std::string get_postproc() const = 0;
};

class CPUPipeBuilder : public PipeBuilder {
  private:
    bool _vaapipostproc_avaliable;
    bool _vapostproc_avaliable;

  public:
    CPUPipeBuilder()
        : _vaapipostproc_avaliable(is_vaapipostproc_available()), _vapostproc_avaliable(is_vapostproc_available()) {
    }

    std::string get_preproc() const override {
        if (_vapostproc_avaliable)
            return std::string(elem::vapostproc) + elem::pipe_separator + elem::caps_system_memory;
        else if (_vaapipostproc_avaliable)
            return std::string(elem::vaapipostproc) + elem::pipe_separator + elem::caps_system_memory;
        return elem::videoconvert;
    }
    std::string get_process() const override {
        return std::string(elem::videoconvert) + elem::pipe_separator + elem::opencv_meta_overlay;
    }
    std::string get_postproc() const override {
        std::string res = elem::videoconvert;
        if (_vapostproc_avaliable)
            res += std::string(elem::pipe_separator) + elem::vapostproc;
        else if (_vaapipostproc_avaliable)
            res += std::string(elem::pipe_separator) + elem::vaapipostproc;
        return res;
    }
};

class GPUPipeBuilder : public PipeBuilder {
  private:
    bool _vaapipostproc_avaliable;
    bool _vapostproc_avaliable;

  public:
    GPUPipeBuilder()
        : _vaapipostproc_avaliable(is_vaapipostproc_available()), _vapostproc_avaliable(is_vapostproc_available()) {
    }

    std::string get_preproc() const override {
        if (_vapostproc_avaliable)
            return std::string(elem::videoconvert) + elem::pipe_separator + elem::vapostproc;
        else if (_vaapipostproc_avaliable)
            return std::string(elem::videoconvert) + elem::pipe_separator + elem::vaapipostproc;
        else
            return std::string(elem::videoconvert);
    }
    std::string get_process() const override {
        return std::string(elem::videoconvert) + elem::pipe_separator + elem::opencv_meta_overlay;
    }
    std::string get_postproc() const override {
        if (_vapostproc_avaliable)
            return std::string(elem::vapostproc) + elem::pipe_separator + elem::videoconvert;
        else if (_vaapipostproc_avaliable)
            return std::string(elem::vaapipostproc) + elem::pipe_separator + elem::videoconvert;
        else
            return elem::videoconvert;
    }
};

class MetaOverlayBinPrivate {
  public:
    static std::shared_ptr<MetaOverlayBinPrivate> unpack(gpointer base) {
        g_assert(GST_META_OVERLAY_BIN(base)->impl != nullptr);
        return GST_META_OVERLAY_BIN(base)->impl;
    }

    MetaOverlayBinPrivate(GstMetaOverlayBin *self, GstProcessBin *base) : _self(self), _base(base) {
    }

    ~MetaOverlayBinPrivate() {
    }

    void get_property(guint prop_id, GValue *value, GParamSpec *pspec) {
        switch (prop_id) {
        case PROP_DEVICE:
            g_value_set_enum(value, _device);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(_base, prop_id, pspec);
            break;
        }
    }

    void set_property(guint prop_id, const GValue *value, GParamSpec *pspec) {
        switch (prop_id) {
        case PROP_DEVICE:
            _device = static_cast<MetaOverlayDevice>(g_value_get_enum(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(_base, prop_id, pspec);
            break;
        }
    }

    GstStateChangeReturn change_state(GstStateChange transition) {
        switch (transition) {
        case GST_STATE_CHANGE_NULL_TO_READY: {
            g_return_val_if_fail(link_elements(), GST_STATE_CHANGE_FAILURE);
        } break;
        default:
            break;
        }

        return GST_ELEMENT_CLASS(meta_overlay_bin_parent_class)->change_state(GST_ELEMENT(_base), transition);
    }

  private:
    GstMetaOverlayBin *_self = nullptr;
    GstProcessBin *_base = nullptr;
    std::unique_ptr<PipeBuilder> pipe_builder;

    // properties
    MetaOverlayDevice _device = DEFAULT_DEVICE;

    bool link_elements() {
        GObject *gobject = G_OBJECT(_base);
        switch (_device) {
        case CPU:
            pipe_builder = std::make_unique<CPUPipeBuilder>();
            break;
        case GPU:
            pipe_builder = std::make_unique<GPUPipeBuilder>();
            break;

        default: {
            GST_ERROR_OBJECT(_base, "Unsupported meta_overlay device");
            return false;
        }
        }

        std::string preprocess, process, postprocess;
        if (dlstreamer::get_property_as_string(gobject, "preprocess") == "NULL")
            preprocess = pipe_builder->get_preproc();

        if (dlstreamer::get_property_as_string(gobject, "process") == "NULL")
            process = pipe_builder->get_process();

        if (dlstreamer::get_property_as_string(gobject, "postprocess") == "NULL")
            postprocess = pipe_builder->get_postproc();

        // TODO set elements via properties?
        return processbin_set_elements_description(_base, preprocess.data(), process.data(), postprocess.data(), "",
                                                   "");
    }
};

static void meta_overlay_bin_init(GstMetaOverlayBin *self) {
    self->impl = std::make_shared<MetaOverlayBinPrivate>(self, &self->process_bin);
}

static void meta_overlay_bin_finalize(GObject *object) {
    GstMetaOverlayBin *self = GST_META_OVERLAY_BIN(object);
    self->impl.reset();

    G_OBJECT_CLASS(meta_overlay_bin_parent_class)->finalize(object);
}

static void meta_overlay_bin_class_init(GstMetaOverlayBinClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    element_class->change_state = [](GstElement *element, GstStateChange transition) {
        return MetaOverlayBinPrivate::unpack(element)->change_state(transition);
    };
    gobject_class->set_property = [](GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
        return MetaOverlayBinPrivate::unpack(object)->set_property(property_id, value, pspec);
    };
    gobject_class->get_property = [](GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
        return MetaOverlayBinPrivate::unpack(object)->get_property(property_id, value, pspec);
    };
    gobject_class->finalize = meta_overlay_bin_finalize;

    gst_element_class_add_pad_template(element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string("video/x-raw(ANY)")));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string("video/x-raw(ANY)")));

    g_object_class_install_property(
        gobject_class, PROP_DEVICE,
        g_param_spec_enum("device", "Target device", "Target device for meta_overlaying", GST_TYPE_META_OVERLAY_DEVICE,
                          DEFAULT_DEVICE, static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    gst_element_class_set_metadata(element_class, META_OVERLAY_BIN_NAME, "video", META_OVERLAY_BIN_DESCRIPTION,
                                   "Intel Corporation");
}
