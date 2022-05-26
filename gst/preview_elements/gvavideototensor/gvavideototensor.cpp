/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"

#include "gvavideototensor.hpp"
#include "preprocessors/ie_preproc.hpp"
#include "preprocessors/opencv_preproc.hpp"
#include "preprocessors/vaapi_preproc.hpp"
#include "preprocessors/vaapi_surface_sharing_preproc.hpp"

#include "dlstreamer/gst/vaapi_context.h"
#include <capabilities/tensor_caps.hpp>
#include <capabilities/video_caps.hpp>
#include <gva_roi_ref_meta.hpp>
#include <gva_utils.h>
#include <meta/gva_buffer_flags.hpp>
#include <model_proc_provider.h>
#include <pre_processor_info_parser.hpp>
#include <scope_guard.h>
#include <tensor_layer_desc.hpp>
#include <utils.h>

#include <inference_backend/logger.h>

#include <gst/video/video.h>

#ifdef ENABLE_VAAPI
#include <vaapi_utils.h>
#endif

#include <deque>

namespace {
constexpr auto UNKNOWN_VALUE_NAME = "unknown";
constexpr auto PRE_PROC_OPENCV_NAME = "opencv";
constexpr auto PRE_PROC_IE_NAME = "ie";
constexpr auto PRE_PROC_VAAPI_NAME = "vaapi";
constexpr auto PRE_PROC_VAAPI_SURFACE_SHARING_NAME = "vaapi-surface-sharing";

constexpr auto DEFAULT_PRE_PROC_BACKEND = IE;
constexpr auto DEFAULT_CROP_ROI = false;

std::string pre_proc_backend_to_string(PreProcBackend backend) {
    switch (backend) {
    case OPENCV:
        return PRE_PROC_OPENCV_NAME;
    case IE:
        return PRE_PROC_IE_NAME;
    case VAAPI_SYSTEM:
        return PRE_PROC_VAAPI_NAME;
    case VAAPI_SURFACE_SHARING:
        return PRE_PROC_VAAPI_SURFACE_SHARING_NAME;
    default:
        return UNKNOWN_VALUE_NAME;
    }
}

#ifdef ENABLE_VAAPI
dlstreamer::VAAPIContextPtr createVaDisplay(GstBaseTransform *base_transform) {
    assert(base_transform);

    std::shared_ptr<dlstreamer::GSTVAAPIContext> display;
    try {
        display = std::make_shared<dlstreamer::GSTVAAPIContext>(base_transform);
        GST_DEBUG_OBJECT(base_transform, "Using shared VADisplay");
        return display;
    } catch (...) {
        GST_DEBUG_OBJECT(base_transform, "Error creating GSTVAAPIContext");
    }

    return vaApiCreateVaDisplay();
}
#endif

void send_preproc_event(GstPad *pad, GstVideoInfo *vinfo, const InferenceEngine::PreProcessInfo &ppinfo,
                        void *vadpy = nullptr) {
    auto event = gva_event_new_preproc_info(vinfo, ppinfo.getResizeAlgorithm(), ppinfo.getColorFormat(), vadpy);
    gst_pad_push_event(pad, event);
}

bool query_model_input(GvaVideoToTensor *self) {
    auto query = gva_query_new_model_input();
    if (!gst_pad_peer_query(GST_BASE_TRANSFORM(self)->srcpad, query)) {
        gst_query_unref(query);
        return false;
    }

    auto ret = gva_query_parse_model_input(query, self->props.model_input);
    gst_query_unref(query);
    return ret;
}

bool preproc_modifies_image(PreProcBackend backend) {
    switch (backend) {
    case OPENCV:
    case VAAPI_SYSTEM:
    case VAAPI_SURFACE_SHARING:
        return true;
    case IE:
        return false;
    default:
        throw std::runtime_error("Unsupported preprocessor type");
    }
}

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

GST_DEBUG_CATEGORY(gva_video_to_tensor_debug_category);
#define GST_CAT_DEFAULT gva_video_to_tensor_debug_category

G_DEFINE_TYPE(GvaVideoToTensor, gva_video_to_tensor, GST_TYPE_BASE_TRANSFORM);

GType gva_video_to_tensor_backend_get_type(void) {
    static const GEnumValue backend_types[] = {
        {OPENCV, "OpenCV", PRE_PROC_OPENCV_NAME},
        {IE, "Inference Engine", PRE_PROC_IE_NAME},
#ifdef ENABLE_VAAPI
        {VAAPI_SYSTEM, "VAAPI", PRE_PROC_VAAPI_NAME},
        {VAAPI_SURFACE_SHARING, "VAAPI Surface Sharing", PRE_PROC_VAAPI_SURFACE_SHARING_NAME},
#endif
        {0, nullptr, nullptr}};

    static GType gva_video_to_tensor_backend_type = g_enum_register_static("GvaVideoToTensorBackend", backend_types);
    return gva_video_to_tensor_backend_type;
}

enum { PROP_0, PROP_MODEL_PROC, PROP_PRE_PROC_BACKEND, PROP_CROP_ROI };

using namespace InferenceBackend;

void _GvaVideoToTensor::init_preprocessor() {
    props.preprocessor.reset();

    switch (props.pre_proc_backend) {
    case OPENCV:
        props.preprocessor.reset(new OpenCVPreProc(props.input_info, props.tensor_caps, props.pre_proc_info));
        break;
    case IE:
        props.preprocessor.reset(new IEPreProc(props.input_info));
        send_preproc_event(GST_BASE_TRANSFORM_SRC_PAD(this), props.input_info,
                           *static_cast<IEPreProc *>(props.preprocessor.get())->info());
        break;
#ifdef ENABLE_VAAPI
    case VAAPI_SYSTEM:
        props.preprocessor.reset(
            new VaapiPreProc(createVaDisplay(&this->base), props.input_info, props.tensor_caps, props.pre_proc_info));
        break;
    case VAAPI_SURFACE_SHARING: {
        auto pre_proc = new VaapiSurfaceSharingPreProc(createVaDisplay(&this->base), props.input_info,
                                                       props.tensor_caps, props.pre_proc_info);
        send_preproc_event(GST_BASE_TRANSFORM_SRC_PAD(this), nullptr, *pre_proc->info(), pre_proc->display());
        props.preprocessor.reset(pre_proc);
        break;
    }
#endif
    default:
        throw std::runtime_error("Unsupported preprocessor type");
    }
}

bool _GvaVideoToTensor::need_preprocessing() const {
    // TODO: more accurate check if we need to do some preprocessing or could run in passthrough mode
    if (props.crop_roi)
        return true;

    if (!props.preprocessor) {
        GST_ERROR_OBJECT(this, "Preprocessor is not initialized. Skip preprocessing");
        return false;
    }

    return props.preprocessor->need_preprocessing();
}

InferenceBackend::MemoryType _GvaVideoToTensor::get_output_mem_type(InferenceBackend::MemoryType input_mem_type) const {
    if (input_mem_type == InferenceBackend::MemoryType::SYSTEM && props.pre_proc_backend != IE &&
        props.pre_proc_backend != OPENCV)
        throw std::invalid_argument("Only ie and opencv preprocessors supported for system memory");

    switch (props.pre_proc_backend) {
    case OPENCV:
    case IE:
        return InferenceBackend::MemoryType::SYSTEM;
#ifdef ENABLE_VAAPI
    case VAAPI_SYSTEM:
        return InferenceBackend::MemoryType::SYSTEM;
    case VAAPI_SURFACE_SHARING:
        return input_mem_type;
#endif
    default:
        throw std::invalid_argument("Unknown memory type");
    }
}

GstFlowReturn _GvaVideoToTensor::send_gap_event(GstBuffer *buf) const {
    ITT_TASK("PUSH GAP EVENT");
    auto gap_event = gst_event_new_gap(GST_BUFFER_PTS(buf), GST_BUFFER_DURATION(buf));
    return gst_pad_push_event(base.srcpad, gap_event) ? GST_BASE_TRANSFORM_FLOW_DROPPED : GST_FLOW_ERROR;
}

GstFlowReturn _GvaVideoToTensor::run_preproc_on_rois(GstBuffer *inbuf, GstBuffer *outbuf, bool only_convert) const {
    if (gst_buffer_get_n_meta(inbuf, GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE) > 1) {
        GST_ERROR_OBJECT(this, "Input buffer should have only one or no ROI meta");
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
            GST_ERROR_OBJECT(this, "Failed to remove ROI meta from input buffer");
            return GST_FLOW_ERROR;
        }
    } else {
        if (!gst_buffer_copy_into(outbuf, inbuf, GST_BUFFER_COPY_FLAGS, 0, static_cast<gsize>(-1))) {
            GST_ERROR_OBJECT(this, "Failed to copy flags from inbuf to outbuf");
            return GST_FLOW_ERROR;
        }

        if (run_preproc(inbuf, outbuf, roi_meta) != GST_FLOW_OK) {
            GST_ERROR_OBJECT(this, "Failed to run preprocessing on ROI");
            return GST_FLOW_ERROR;
        }

        auto ref_meta = GVA_ROI_REF_META_ADD(outbuf);
        ref_meta->reference_roi_id = roi_meta->id;
        get_object_id(roi_meta, &ref_meta->object_id);
    }

    return GST_FLOW_OK;
}

GstFlowReturn _GvaVideoToTensor::run_preproc(GstBuffer *inbuf, GstBuffer *outbuf,
                                             GstVideoRegionOfInterestMeta *roi) const {
    if (!props.preprocessor) {
        GST_ERROR_OBJECT(this, "Preprocessor is not initialized");
        return GST_FLOW_ERROR;
    }

    try {
        props.preprocessor->process(inbuf, outbuf, roi);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(this, "Error during transforming input buffer: %s", Utils::createNestedErrorMsg(e).c_str());
        return GST_FLOW_ERROR;
    }

    return GST_FLOW_OK;
}

static void gva_video_to_tensor_init(GvaVideoToTensor *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // Initialize C++ structure with new
    new (&self->props) GvaVideoToTensor::_Props();
}

static void gva_video_to_tensor_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GvaVideoToTensor *self = GVA_VIDEO_TO_TENSOR(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (prop_id) {
    case PROP_MODEL_PROC:
        self->props.model_proc = g_value_get_string(value);
        break;
    case PROP_PRE_PROC_BACKEND:
        self->props.pre_proc_backend = static_cast<PreProcBackend>(g_value_get_enum(value));
        break;
    case PROP_CROP_ROI:
        self->props.crop_roi = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gva_video_to_tensor_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GvaVideoToTensor *self = GVA_VIDEO_TO_TENSOR(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (prop_id) {
    case PROP_MODEL_PROC:
        g_value_set_string(value, self->props.model_proc.c_str());
        break;
    case PROP_PRE_PROC_BACKEND:
        g_value_set_enum(value, self->props.pre_proc_backend);
        break;
    case PROP_CROP_ROI:
        g_value_set_boolean(value, self->props.crop_roi);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gva_video_to_tensor_dispose(GObject *object) {
    GvaVideoToTensor *self = GVA_VIDEO_TO_TENSOR(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    G_OBJECT_CLASS(gva_video_to_tensor_parent_class)->dispose(object);
}

static void gva_video_to_tensor_finalize(GObject *object) {
    GvaVideoToTensor *self = GVA_VIDEO_TO_TENSOR(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    gst_video_info_free(self->props.input_info);
    // Destroy C++ structure manually
    self->props.~_Props();

    G_OBJECT_CLASS(gva_video_to_tensor_parent_class)->finalize(object);
}

static gboolean gva_video_to_tensor_set_caps(GstBaseTransform *base, GstCaps *incaps, GstCaps *outcaps) {
    GvaVideoToTensor *self = GVA_VIDEO_TO_TENSOR(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    if (gst_caps_get_size(incaps) > 1 || gst_caps_get_size(outcaps) > 1) {
        GST_ERROR_OBJECT(self, "Only single capabilities on each pad is supported.");
        return false;
    }

    /* set input tensor info */
    if (!self->props.input_info) {
        self->props.input_info = gst_video_info_new();
    }

    if (!gst_video_info_from_caps(self->props.input_info, incaps)) {
        GST_ERROR_OBJECT(self, "Failed to get video info from caps");
        return false;
    }

    try {
        self->props.tensor_caps = TensorCaps::FromCaps(outcaps);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Failed to parse tensor capabilities: %s", Utils::createNestedErrorMsg(e).c_str());
        return false;
    }

    try {
        self->init_preprocessor();
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Failed to create preprocessor: %s", Utils::createNestedErrorMsg(e).c_str());
        return false;
    }

    gst_base_transform_set_passthrough(base, !self->need_preprocessing());

    return true;
}

static gboolean gva_video_to_tensor_sink_event(GstBaseTransform *base, GstEvent *event) {
    GvaVideoToTensor *self = GVA_VIDEO_TO_TENSOR(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    if (GST_EVENT_TYPE(event) == GST_EVENT_EOS && self->props.preprocessor)
        self->props.preprocessor->flush();

    return GST_BASE_TRANSFORM_CLASS(gva_video_to_tensor_parent_class)->sink_event(base, event);
}

static gboolean gva_video_to_tensor_start(GstBaseTransform *base) {
    GvaVideoToTensor *self = GVA_VIDEO_TO_TENSOR(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    GST_INFO_OBJECT(self, "%s parameters:\n -- Model proc: %s\n -- Preprocessing backend: %s\n",
                    GST_ELEMENT_NAME(GST_ELEMENT_CAST(self)), self->props.model_proc.c_str(),
                    pre_proc_backend_to_string(self->props.pre_proc_backend).c_str());

    if (!self->props.model_proc.empty()) {
        try {
            ModelProcProvider model_proc_provider;
            model_proc_provider.readJsonFile(self->props.model_proc);
            self->props.input_processor_info = model_proc_provider.parseInputPreproc();
            for (const auto &i : self->props.input_processor_info) {
                if (i->format == "image") {
                    self->props.pre_proc_info = PreProcParamsParser(i->params).parse();
                    break;
                }
            }
            // TODO: do we need warning user if no input provided in model-proc
        } catch (const std::exception &e) {
            GST_ERROR_OBJECT(self, "Failed to parse model proc file: %s", Utils::createNestedErrorMsg(e).c_str());
            return false;
        }
    }

    // For IE, we don't need to modify buffer memory, only attach meta, so use transform_ip
    gst_base_transform_set_in_place(base, self->props.pre_proc_backend == IE);

    return true;
}

static gboolean gva_video_to_tensor_stop(GstBaseTransform *base) {
    GvaVideoToTensor *self = GVA_VIDEO_TO_TENSOR(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return true;
}

GstCaps *gva_video_to_tensor_transform_caps(GstBaseTransform *base, GstPadDirection direction, GstCaps *caps,
                                            GstCaps *filter) {
    GvaVideoToTensor *self = GVA_VIDEO_TO_TENSOR(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    GstCaps *ret = nullptr;

    auto srccaps = gst_pad_get_pad_template_caps(GST_BASE_TRANSFORM_SRC_PAD(base));
    auto sinkcaps = gst_pad_get_pad_template_caps(GST_BASE_TRANSFORM_SINK_PAD(base));

    switch (direction) {
    case GST_PAD_SINK: {
        if (gst_caps_can_intersect(caps, sinkcaps)) {
            try {
                query_model_input(self);
                std::string layer_name = self->props.model_input ? self->props.model_input.layer_name : std::string();
                auto mem_type = self->get_output_mem_type(get_memory_type_from_caps(caps));
                if (preproc_modifies_image(self->props.pre_proc_backend)) {
                    if (self->props.model_input) {
                        auto &model_input = self->props.model_input;
                        ret = TensorCaps::ToCaps(
                            TensorCaps(mem_type, model_input.precision, model_input.layout, model_input.dims));
                    }
                } else if (gst_caps_is_fixed(caps)) {
                    GstVideoInfo *video_info = gst_video_info_new();
                    if (gst_video_info_from_caps(video_info, caps)) {
                        std::vector<size_t> dims_vec = {1u, get_channels_count(GST_VIDEO_INFO_FORMAT(video_info)),
                                                        safe_convert<size_t>(GST_VIDEO_INFO_HEIGHT(video_info)),
                                                        safe_convert<size_t>(GST_VIDEO_INFO_WIDTH(video_info))};
                        ret = TensorCaps::ToCaps(TensorCaps(mem_type, Precision::U8, Layout::NCHW, dims_vec));
                    }
                    gst_video_info_free(video_info);
                }
            } catch (const std::exception &e) {
                GST_ERROR_OBJECT(self, "Failed to create tensor caps: %s", Utils::createNestedErrorMsg(e).c_str());
                ret = gst_caps_new_empty();
            }

            if (!ret)
                ret = gst_caps_copy(srccaps);
        } else {
            ret = gst_caps_new_empty();
        }
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

    GST_DEBUG_OBJECT(base, "transformed %" GST_PTR_FORMAT, ret);

    if (filter) {
        GstCaps *intersection;

        GST_DEBUG_OBJECT(base, "Using filter caps %" GST_PTR_FORMAT, filter);

        intersection = gst_caps_intersect_full(filter, ret, GST_CAPS_INTERSECT_FIRST);
        gst_caps_unref(ret);
        ret = intersection;

        GST_DEBUG_OBJECT(base, "Intersection %" GST_PTR_FORMAT, ret);
    }

    gst_caps_unref(srccaps);
    gst_caps_unref(sinkcaps);

    return ret;
}

static GstFlowReturn gva_video_to_tensor_transform(GstBaseTransform *base, GstBuffer *inbuf, GstBuffer *outbuf) {
    GvaVideoToTensor *self = GVA_VIDEO_TO_TENSOR(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // If preprocessing is not needed then basetranform should work in passthrough mode
    g_assert(self->need_preprocessing());

    if (self->props.crop_roi) {
        GST_DEBUG_OBJECT(self, "Transform buffer with ROIs: ts=%" GST_TIME_FORMAT,
                         GST_TIME_ARGS(GST_BUFFER_PTS(inbuf)));
        return self->run_preproc_on_rois(inbuf, outbuf);
    }

    // TODO: it's a hint for meta_aggregate in case if we're running on full frame
    // for ROIs we assume that roi_split will mark last roi
    gst_buffer_set_flags(outbuf, static_cast<GstBufferFlags>(GVA_BUFFER_FLAG_LAST_ROI_ON_FRAME));

    GST_DEBUG_OBJECT(self, "Transform buffer: ts=%" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(inbuf)));
    return self->run_preproc(inbuf, outbuf);
}

static GstFlowReturn gva_video_to_tensor_transform_ip(GstBaseTransform *base, GstBuffer *buf) {
    ITT_TASK(std::string(GST_ELEMENT_NAME(base)) + " " + __FUNCTION__);

    GvaVideoToTensor *self = GVA_VIDEO_TO_TENSOR(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // If preprocessing is not needed then basetranform should work in passthrough mode
    g_assert(self->need_preprocessing());

    if (self->props.crop_roi) {
        GST_DEBUG_OBJECT(self, "TransformIP buffer with ROIs: ts=%" GST_TIME_FORMAT,
                         GST_TIME_ARGS(GST_BUFFER_PTS(buf)));
        return self->run_preproc_on_rois(buf, nullptr, true);
    }

    remove_all_rois_from_buffer(buf);
    // TODO: it's a hint for meta_aggregate in case if we're running on full frame
    // for ROIs we assume that roi_split will mark last roi
    gst_buffer_set_flags(buf, static_cast<GstBufferFlags>(GVA_BUFFER_FLAG_LAST_ROI_ON_FRAME));

    GST_DEBUG_OBJECT(self, "TransformIP buffer: ts=%" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(buf)));
    return self->run_preproc(buf);
}

static gboolean gva_video_to_tensor_transform_size(GstBaseTransform *base, GstPadDirection /*direction*/,
                                                   GstCaps * /*caps*/, gsize /*size*/, GstCaps * /*othercaps*/,
                                                   gsize *othersize) {
    GvaVideoToTensor *self = GVA_VIDEO_TO_TENSOR(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // transform_size shouldn't be called in this case
    g_assert(gst_base_transform_is_passthrough(base) == false);
    g_assert(gst_base_transform_is_in_place(base) == false);

    if (!self->props.preprocessor) {
        GST_ERROR_OBJECT(self, "Failed to calculate out buffer size: Preprocessor is not initialized");
        return false;
    }

    try {
        *othersize = self->props.preprocessor->output_size();
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Failed to calculate output buffer size: %s", Utils::createNestedErrorMsg(e).c_str());
        return false;
    }

    return true;
}

static void gva_video_to_tensor_class_init(GvaVideoToTensorClass *klass) {
    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = gva_video_to_tensor_set_property;
    gobject_class->get_property = gva_video_to_tensor_get_property;
    gobject_class->dispose = gva_video_to_tensor_dispose;
    gobject_class->finalize = gva_video_to_tensor_finalize;

    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->set_caps = gva_video_to_tensor_set_caps;
    base_transform_class->sink_event = gva_video_to_tensor_sink_event;
    base_transform_class->transform_caps = gva_video_to_tensor_transform_caps;
    base_transform_class->start = gva_video_to_tensor_start;
    base_transform_class->stop = gva_video_to_tensor_stop;
    base_transform_class->transform_size = gva_video_to_tensor_transform_size;
    base_transform_class->transform = gva_video_to_tensor_transform;
    base_transform_class->transform_ip = gva_video_to_tensor_transform_ip;

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, GVA_VIDEO_TO_TENSOR_NAME, "application",
                                          GVA_VIDEO_TO_TENSOR_DESCRIPTION, "Intel Corporation");

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                                            gst_caps_from_string(GVA_TENSORS_CAPS GVA_VAAPI_TENSORS_CAPS)));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(GVA_VIDEO_CAPS)));

    g_object_class_install_property(gobject_class, PROP_MODEL_PROC,
                                    g_param_spec_string("model-proc", "Model proc", "Path to model proc file", NULL,
                                                        (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_PRE_PROC_BACKEND,
        g_param_spec_enum("pre-process-backend", "Preproc backend", "Preprocessing backend type",
                          GST_TYPE_GVA_VIDEO_TO_TENSOR_BACKEND, DEFAULT_PRE_PROC_BACKEND,
                          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));
    g_object_class_install_property(gobject_class, PROP_CROP_ROI,
                                    g_param_spec_boolean("crop-roi", "Crop ROI", "Crop image by ROI meta",
                                                         DEFAULT_CROP_ROI,
                                                         (GParamFlags)(G_PARAM_READWRITE | G_PARAM_CONSTRUCT)));
}
