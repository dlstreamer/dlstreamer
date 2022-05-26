/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"

#include "preproc_base.hpp"

#include <capabilities/video_caps.hpp>
#include <gva_roi_ref_meta.hpp>
#include <gva_utils.h>
#include <meta/gva_buffer_flags.hpp>
#include <model_proc_provider.h>
#include <pre_processor_info_parser.hpp>
#include <utils.h>

#include <inference_backend/logger.h>

#include <gst/video/video.h>

#include <memory>

namespace {
constexpr auto DEFAULT_CROP_ROI = false;

void remove_all_rois_from_buffer(GstBuffer *buffer) {
    gst_buffer_foreach_meta(buffer,
                            [](GstBuffer *, GstMeta **meta, gpointer) -> int {
                                if ((*meta)->info->api == GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE)
                                    *meta = nullptr;
                                return true;
                            },
                            nullptr);
}

} // namespace

GST_DEBUG_CATEGORY(gva_preproc_base_debug_category);
#define GST_CAT_DEFAULT gva_preproc_base_debug_category

enum { PROP_0, PROP_MODEL_PROC, PROP_CROP_ROI };

class GvaPreprocBasePrivate {
  public:
    GvaPreprocBasePrivate(GstBaseTransform *base) : _base(base) {
        gst_base_transform_set_in_place(_base, false);
    }

    ~GvaPreprocBasePrivate() = default;

    void set_preproc_elem(IPreProcElem *elem) {
        _preproc_elem = elem;
    }

    bool start() {
        g_assert(_preproc_elem && "Preproc element implementation must be set");
        GST_DEBUG_OBJECT(_base, "%s", __FUNCTION__);

        GST_INFO_OBJECT(_base, "%s parameters:\n -- Model proc: %s\n", GST_ELEMENT_NAME(GST_ELEMENT_CAST(_base)),
                        _model_proc.c_str());

        if (!_model_proc.empty()) {
            try {
                ModelProcProvider model_proc_provider;
                model_proc_provider.readJsonFile(_model_proc);
                auto input_processor_info = model_proc_provider.parseInputPreproc();
                for (const auto &i : input_processor_info) {
                    if (i->format == "image") {
                        _pre_proc_info = PreProcParamsParser(i->params).parse();
                        break;
                    }
                }
            } catch (const std::exception &e) {
                GST_ERROR_OBJECT(_base, "Failed to parse model proc file: %s", Utils::createNestedErrorMsg(e).c_str());
                return false;
            }
        }

        return _preproc_elem->start();
    }

    bool stop() {
        return _preproc_elem->stop();
    }

    void set_property(guint prop_id, const GValue *value, GParamSpec *pspec) {
        GST_DEBUG_OBJECT(_base, "%s", __FUNCTION__);

        if (_preproc_elem->set_property(prop_id, value))
            return;

        switch (prop_id) {
        case PROP_MODEL_PROC:
            _model_proc = g_value_get_string(value);
            break;
        case PROP_CROP_ROI:
            _crop_roi = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(_base, prop_id, pspec);
            break;
        }
    }

    void get_property(guint prop_id, GValue *value, GParamSpec *pspec) {
        GST_DEBUG_OBJECT(_base, "%s", __FUNCTION__);

        if (_preproc_elem->get_property(prop_id, value))
            return;

        switch (prop_id) {
        case PROP_MODEL_PROC:
            g_value_set_string(value, _model_proc.c_str());
            break;
        case PROP_CROP_ROI:
            g_value_set_boolean(value, _crop_roi);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(_base, prop_id, pspec);
            break;
        }
    }

    bool set_caps(GstCaps *incaps, GstCaps *outcaps) {
        GST_DEBUG_OBJECT(_base, "%s", __FUNCTION__);

        if (gst_caps_get_size(incaps) > 1 || gst_caps_get_size(outcaps) > 1) {
            GST_ERROR_OBJECT(_base, "Only single capabilities on each pad is supported.");
            return false;
        }

        try {
            _preproc_elem->init_preprocessing(_pre_proc_info, incaps, outcaps);
        } catch (const std::exception &e) {
            GST_ERROR_OBJECT(_base, "Failed to init preprocessing: %s", Utils::createNestedErrorMsg(e).c_str());
            return false;
        }

        gst_base_transform_set_passthrough(_base, !_preproc_elem->need_preprocessing());

        return true;
    }

    bool sink_event(GstEvent *event) {
        GST_DEBUG_OBJECT(_base, "%s", __FUNCTION__);

        if (GST_EVENT_TYPE(event) == GST_EVENT_EOS)
            _preproc_elem->flush();

        return get_base_transform_class()->sink_event(_base, event);
    }

    GstCaps *transform_caps(GstPadDirection direction, GstCaps *caps, GstCaps *filter) {
        GST_DEBUG_OBJECT(_base, "%s", __FUNCTION__);

        GstCaps *ret = nullptr;

        auto srccaps = gst_pad_get_pad_template_caps(GST_BASE_TRANSFORM_SRC_PAD(_base));
        auto sinkcaps = gst_pad_get_pad_template_caps(GST_BASE_TRANSFORM_SINK_PAD(_base));

        switch (direction) {
        case GST_PAD_SINK: {
            if (gst_caps_can_intersect(caps, sinkcaps))
                ret = gst_caps_copy(srccaps);
            else
                ret = gst_caps_new_empty();
            break;
        }
        case GST_PAD_SRC: {
            if (gst_caps_can_intersect(caps, srccaps))
                ret = gst_caps_copy(sinkcaps);
            else
                ret = gst_caps_new_empty();
            break;
        }
        default:
            g_assert_not_reached();
        }

        GST_DEBUG_OBJECT(_base, "transformed %" GST_PTR_FORMAT, ret);

        if (filter) {
            GstCaps *intersection;

            GST_DEBUG_OBJECT(_base, "Using filter caps %" GST_PTR_FORMAT, filter);

            intersection = gst_caps_intersect_full(filter, ret, GST_CAPS_INTERSECT_FIRST);
            gst_caps_unref(ret);
            ret = intersection;

            GST_DEBUG_OBJECT(_base, "Intersection %" GST_PTR_FORMAT, ret);
        }

        gst_caps_unref(srccaps);
        gst_caps_unref(sinkcaps);

        return ret;
    }

    GstCaps *fixate_caps(GstPadDirection direction, GstCaps *caps, GstCaps *othercaps) {
        // TODO: check logic
        auto result = gst_caps_intersect(othercaps, caps);
        if (gst_caps_is_empty(result)) {
            gst_caps_unref(result);
            result = othercaps;
        } else {
            gst_caps_unref(othercaps);
        }
        result = gst_caps_make_writable(result);
        result = gst_caps_fixate(result);
        if (direction == GST_PAD_SINK) {
            if (gst_caps_is_subset(caps, result)) {
                gst_caps_replace(&result, caps);
            } else {
                // Fixate framerate
                auto caps_s = gst_caps_get_structure(caps, 0);
                gint fps_n, fps_d;
                if (gst_structure_get_fraction(caps_s, "framerate", &fps_n, &fps_d)) {
                    gst_caps_set_simple(othercaps, "framerate", GST_TYPE_FRACTION, fps_n, fps_d, nullptr);
                }
            }
        }
        return result;
    }

    GstFlowReturn transform(GstBuffer *inbuf, GstBuffer *outbuf) {
        GST_DEBUG_OBJECT(_base, "%s", __FUNCTION__);

        // If preprocessing is not needed then basetranform should work in passthrough mode
        g_assert(_preproc_elem->need_preprocessing());

        if (_crop_roi) {
            GST_DEBUG_OBJECT(_base, "Transform buffer with ROIs: ts=%" GST_TIME_FORMAT,
                             GST_TIME_ARGS(GST_BUFFER_PTS(inbuf)));
            return run_preproc_on_rois(inbuf, outbuf);
        }

        // TODO: it's a hint for meta_aggregate in case if we're running on full frame
        // for ROIs we assume that roi_split will mark last roi
        gst_buffer_set_flags(outbuf, static_cast<GstBufferFlags>(GVA_BUFFER_FLAG_LAST_ROI_ON_FRAME));

        GST_DEBUG_OBJECT(_base, "Transform buffer: ts=%" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(inbuf)));
        return _preproc_elem->run_preproc(inbuf, outbuf);
    }

    GstFlowReturn transform_ip(GstBuffer *buf) {
        ITT_TASK(std::string(GST_ELEMENT_NAME(_base)) + " " + __FUNCTION__);
        GST_DEBUG_OBJECT(_base, "%s", __FUNCTION__);

        // If preprocessing is not needed then basetranform should work in passthrough mode
        g_assert(_preproc_elem->need_preprocessing());

        if (_crop_roi) {
            GST_DEBUG_OBJECT(_base, "TransformIP buffer with ROIs: ts=%" GST_TIME_FORMAT,
                             GST_TIME_ARGS(GST_BUFFER_PTS(buf)));
            return run_preproc_on_rois(buf, nullptr, true);
        }

        remove_all_rois_from_buffer(buf);
        // TODO: it's a hint for meta_aggregate in case if we're running on full frame
        // for ROIs we assume that roi_split will mark last roi
        gst_buffer_set_flags(buf, static_cast<GstBufferFlags>(GVA_BUFFER_FLAG_LAST_ROI_ON_FRAME));

        GST_DEBUG_OBJECT(_base, "TransformIP buffer: ts=%" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(buf)));
        return _preproc_elem->run_preproc(buf);
    }

    bool transform_size(GstPadDirection direction, GstCaps *caps, gsize size, GstCaps *othercaps, gsize *othersize) {
        return _preproc_elem->transform_size(direction, caps, size, othercaps, othersize);
    }

  protected:
    GstFlowReturn send_gap_event(GstBuffer *buf) const {
        ITT_TASK("PUSH GAP EVENT");
        auto gap_event = gst_event_new_gap(GST_BUFFER_PTS(buf), GST_BUFFER_DURATION(buf));
        return gst_pad_push_event(_base->srcpad, gap_event) ? GST_BASE_TRANSFORM_FLOW_DROPPED : GST_FLOW_ERROR;
    }

    GstFlowReturn run_preproc_on_rois(GstBuffer *inbuf, GstBuffer *outbuf = nullptr, bool only_convert = false) const {
        if (gst_buffer_get_n_meta(inbuf, GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE) > 1) {
            GST_ERROR_OBJECT(_base, "Input buffer should have only one or no ROI meta");
            return GST_FLOW_ERROR;
        }

        auto roi_meta = gst_buffer_get_video_region_of_interest_meta(inbuf);
        if (!roi_meta)
            return send_gap_event(inbuf);

        if (only_convert) {
            // Convert ROI to Crop meta
            auto crop = gst_buffer_add_video_crop_meta(inbuf);
            crop->x = roi_meta->x;
            crop->y = roi_meta->y;
            crop->width = roi_meta->w;
            crop->height = roi_meta->h;

            auto ref_meta = GVA_ROI_REF_META_ADD(inbuf);
            ref_meta->reference_roi_id = roi_meta->id;
            get_object_id(roi_meta, &ref_meta->object_id);

            if (!gst_buffer_remove_meta(inbuf, GST_META_CAST(roi_meta))) {
                GST_ERROR_OBJECT(_base, "Failed to remove ROI meta from input buffer");
                return GST_FLOW_ERROR;
            }
        } else {
            if (!gst_buffer_copy_into(outbuf, inbuf, GST_BUFFER_COPY_FLAGS, 0, static_cast<gsize>(-1))) {
                GST_ERROR_OBJECT(_base, "Failed to copy flags from inbuf to outbuf");
                return GST_FLOW_ERROR;
            }

            if (_preproc_elem->run_preproc(inbuf, outbuf, roi_meta) != GST_FLOW_OK) {
                GST_ERROR_OBJECT(_base, "Failed to run preprocessing on ROI");
                return GST_FLOW_ERROR;
            }

            auto ref_meta = GVA_ROI_REF_META_ADD(outbuf);
            ref_meta->reference_roi_id = roi_meta->id;
            get_object_id(roi_meta, &ref_meta->object_id);
        }

        return GST_FLOW_OK;
    }

    GstBaseTransformClass *get_base_transform_class() const;

  protected:
    GstBaseTransform *_base;
    IPreProcElem *_preproc_elem = nullptr;
    InferenceBackend::InputImageLayerDesc::Ptr _pre_proc_info;
    std::string _model_proc;
    bool _crop_roi = DEFAULT_CROP_ROI;
};

// FIXME: we can't use macros because this type is in static library which is linked by plugins
// All such plugins will try to register GvaPrepocBase type and only first one will be successful
// So we add additional check here if type is already registered: GType _type = g_type_from_name("GvaPreprocBase");

// G_DEFINE_TYPE_EXTENDED(GvaPreprocBase, gva_preproc_base, GST_TYPE_BASE_TRANSFORM, G_TYPE_FLAG_ABSTRACT,
//                        G_ADD_PRIVATE(GvaPreprocBase);
//                        GST_DEBUG_CATEGORY_INIT(gva_preproc_base_debug_category, "gvapreprocbase", 0,
//                                                "debug category for gvapreprocbase element"));

// G_DEFINE_TYPE_EXTENDED expansion
static gpointer gva_preproc_base_parent_class = NULL;
static gint private_offset = 0;

static void gva_preproc_base_class_init(GvaPreprocBaseClass *klass);
static void gva_preproc_base_init(GvaPreprocBase *trans);

GType gva_preproc_base_get_type(void) {
    static gsize gva_preproc_base_type = 0;

    if (g_once_init_enter(&gva_preproc_base_type)) {
        GType _type = g_type_from_name("GvaPreprocBase");
        static const GTypeInfo gva_preproc_base_info = {sizeof(GvaPreprocBaseClass),
                                                        NULL,
                                                        NULL,
                                                        (GClassInitFunc)gva_preproc_base_class_init,
                                                        NULL,
                                                        NULL,
                                                        sizeof(GvaPreprocBase),
                                                        0,
                                                        (GInstanceInitFunc)gva_preproc_base_init,
                                                        nullptr};
        if (!_type) {
            _type = g_type_register_static(GST_TYPE_BASE_TRANSFORM, "GvaPreprocBase", &gva_preproc_base_info,
                                           G_TYPE_FLAG_ABSTRACT);

            private_offset = g_type_add_instance_private(_type, sizeof(GvaPreprocBasePrivate));
            GST_DEBUG_CATEGORY_INIT(gva_preproc_base_debug_category, "gvapreprocbase", 0,
                                    "debug category for gvapreprocbase element");
        }

        g_once_init_leave(&gva_preproc_base_type, _type);
    }
    return gva_preproc_base_type;
}

static inline gpointer gva_preproc_base_get_instance_private(GvaPreprocBase *self) {
    return (G_STRUCT_MEMBER_P(self, private_offset));
}

// End G_DEFINE_TYPE_EXTENDED expansion

GstBaseTransformClass *GvaPreprocBasePrivate::get_base_transform_class() const {
    return GST_BASE_TRANSFORM_CLASS(gva_preproc_base_parent_class);
}

void GvaPreprocBase::set_preproc_elem(IPreProcElem *elem) {
    impl->set_preproc_elem(elem);
}

static void gva_preproc_base_init(GvaPreprocBase *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // Initialization of private data
    auto *priv_memory = gva_preproc_base_get_instance_private(self);
    self->impl = new (priv_memory) GvaPreprocBasePrivate(&self->base);
}

static void gva_preproc_base_finalize(GObject *object) {
    GvaPreprocBase *self = GVA_PREPROC_BASE(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    if (self->impl) {
        self->impl->~GvaPreprocBasePrivate();
        self->impl = nullptr;
    }

    G_OBJECT_CLASS(gva_preproc_base_parent_class)->finalize(object);
}

static void gva_preproc_base_class_init(GvaPreprocBaseClass *klass) {
    // G_DEFINE_TYPE_EXTENDED expansion
    if (private_offset != 0)
        g_type_class_adjust_private_offset(klass, &private_offset);
    gva_preproc_base_parent_class = g_type_class_peek_parent(klass);
    // End G_DEFINE_TYPE_EXTENDED expansion

    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = gva_preproc_base_finalize;
    gobject_class->set_property = [](GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
        GVA_PREPROC_BASE(object)->impl->set_property(prop_id, value, pspec);
    };
    gobject_class->get_property = [](GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
        GVA_PREPROC_BASE(object)->impl->get_property(prop_id, value, pspec);
    };

    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->set_caps = [](GstBaseTransform *base, GstCaps *incaps, GstCaps *outcaps) -> gboolean {
        return GVA_PREPROC_BASE(base)->impl->set_caps(incaps, outcaps);
    };
    base_transform_class->sink_event = [](GstBaseTransform *base, GstEvent *event) -> gboolean {
        return GVA_PREPROC_BASE(base)->impl->sink_event(event);
    };
    base_transform_class->transform_caps = [](GstBaseTransform *base, GstPadDirection direction, GstCaps *caps,
                                              GstCaps *filter) {
        return GVA_PREPROC_BASE(base)->impl->transform_caps(direction, caps, filter);
    };
    base_transform_class->fixate_caps = [](GstBaseTransform *base, GstPadDirection direction, GstCaps *caps,
                                           GstCaps *othercaps) {
        return GVA_PREPROC_BASE(base)->impl->fixate_caps(direction, caps, othercaps);
    };
    base_transform_class->start = [](GstBaseTransform *base) -> gboolean {
        return GVA_PREPROC_BASE(base)->impl->start();
    };
    base_transform_class->stop = [](GstBaseTransform *base) -> gboolean {
        return GVA_PREPROC_BASE(base)->impl->stop();
    };
    base_transform_class->transform_size = [](GstBaseTransform *base, GstPadDirection direction, GstCaps *caps,
                                              gsize size, GstCaps *othercaps, gsize *othersize) -> gboolean {
        return GVA_PREPROC_BASE(base)->impl->transform_size(direction, caps, size, othercaps, othersize);
    };
    base_transform_class->transform = [](GstBaseTransform *base, GstBuffer *inbuf, GstBuffer *outbuf) {
        return GVA_PREPROC_BASE(base)->impl->transform(inbuf, outbuf);
    };
    base_transform_class->transform_ip = [](GstBaseTransform *base, GstBuffer *buf) {
        return GVA_PREPROC_BASE(base)->impl->transform_ip(buf);
    };
    base_transform_class->transform_ip_on_passthrough = false;

    g_object_class_install_property(gobject_class, PROP_MODEL_PROC,
                                    g_param_spec_string("model-proc", "Model proc", "Path to model proc file", NULL,
                                                        (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(gobject_class, PROP_CROP_ROI,
                                    g_param_spec_boolean("crop-roi", "Crop ROI", "Crop image by ROI meta",
                                                         DEFAULT_CROP_ROI,
                                                         (GParamFlags)(G_PARAM_READWRITE | G_PARAM_CONSTRUCT)));
}
