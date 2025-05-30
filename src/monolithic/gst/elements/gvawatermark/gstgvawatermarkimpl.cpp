/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvawatermarkimpl.h"
#include "gvawatermarkcaps.h"

#include <gst/allocators/gstdmabuf.h>
#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video-color.h>
#include <gst/video/video-info.h>

#include <inference_backend/logger.h>
#include <safe_arithmetic.hpp>

#include "gva_caps.h"
#include "gva_utils.h"
#include "inference_backend/buffer_mapper.h"
#include "so_loader.h"
#include "utils.h"
#include "video_frame.h"
#include <dlstreamer/gst/context.h>
#include <dlstreamer/gst/frame.h>
#include <dlstreamer/memory_mapper_factory.h>
#ifdef ENABLE_VAAPI
#include <dlstreamer/vaapi/mappers/vaapi_to_dma.h>
#endif

#include "renderer/color_converter.h"
#include "renderer/cpu/create_renderer.h"

#include <exception>
#include <string>
#include <typeinfo>

#define ELEMENT_LONG_NAME "Implementation for detection/classification/recognition results labeling"
#define ELEMENT_DESCRIPTION "Implements gstgvawatermark element functionality."

GST_DEBUG_CATEGORY_STATIC(gst_gva_watermark_impl_debug_category);
#define GST_CAT_DEFAULT gst_gva_watermark_impl_debug_category

#define DEFAULT_DEVICE nullptr

typedef enum { DEVICE_CPU, DEVICE_GPU, DEVICE_GPU_AUTOSELECTED } DEVICE_SELECTOR;

namespace {

const std::vector<Color> color_table = {Color(255, 0, 0),   Color(0, 255, 0),   Color(0, 0, 255),   Color(255, 255, 0),
                                        Color(0, 255, 255), Color(255, 0, 255), Color(255, 170, 0), Color(255, 0, 170),
                                        Color(0, 255, 170), Color(170, 255, 0), Color(170, 0, 255), Color(0, 170, 255),
                                        Color(255, 85, 0),  Color(85, 255, 0),  Color(0, 255, 85),  Color(0, 85, 255),
                                        Color(85, 0, 255),  Color(255, 0, 85)};

Color indexToColor(size_t index) {
    return color_table[index % color_table.size()];
}

void clip_rect(double &x, double &y, double &w, double &h, GstVideoInfo *info) {
    x = (x < 0) ? 0 : (x > info->width) ? (info->width - 1) : x;
    y = (y < 0) ? 0 : (y > info->height) ? (info->height - 1) : y;
    w = (w < 0) ? 0 : (x + w > info->width) ? (info->width - 1) - x : w;
    h = (h < 0) ? 0 : (y + h > info->height) ? (info->height - 1) - y : h;
}

void appendStr(std::ostringstream &oss, const std::string &s, char delim = ' ') {
    if (!s.empty()) {
        oss << s << delim;
    }
}

InferenceBackend::MemoryType memoryTypeFromCaps(GstCaps *caps) {
    const auto caps_feature = get_caps_feature(caps);
    switch (caps_feature) {
    case SYSTEM_MEMORY_CAPS_FEATURE:
        return InferenceBackend::MemoryType::SYSTEM;

    case VA_SURFACE_CAPS_FEATURE:
    case VA_MEMORY_CAPS_FEATURE:
        return InferenceBackend::MemoryType::VAAPI;

    case DMA_BUF_CAPS_FEATURE:
        return InferenceBackend::MemoryType::DMA_BUFFER;

    default:
        GST_ERROR("Unknown memory caps property: '%s'", gst_caps_to_string(caps));
        return InferenceBackend::MemoryType::ANY;
    }
}

dlstreamer::MemoryMapperPtr createMapperToDMA(InferenceBackend::MemoryType in_mem_type,
                                              dlstreamer::ContextPtr context) {
#ifdef ENABLE_VAAPI
    auto in_mapper = BufferMapperFactory::createMapper(in_mem_type, context);
    if (in_mem_type == InferenceBackend::MemoryType::DMA_BUFFER)
        return in_mapper;

    if (in_mem_type != InferenceBackend::MemoryType::VAAPI)
        throw std::runtime_error("Unsupported input memory type for DMA FD conversion");

    // In case of VAAPI memory create chain of mappers GST -> VAAPI -> DMA
    auto vaapi_to_dma = std::make_shared<dlstreamer::MemoryMapperVAAPIToDMA>(context, nullptr);
    return std::make_shared<dlstreamer::MemoryMapperChain>(
        dlstreamer::MemoryMapperChain{std::move(in_mapper), std::move(vaapi_to_dma)});
#else
    UNUSED(in_mem_type);
    UNUSED(context);
    throw std::runtime_error("VAAPI disabled");
    return nullptr;
#endif
}

} // namespace

struct Impl {
    Impl(GstVideoInfo *info, DEVICE_SELECTOR device, InferenceBackend::MemoryType mem_type,
         dlstreamer::ContextPtr context, bool obb);
    bool render(GstBuffer *buffer);
    const std::string &getBackendType() const {
        return _backend_type;
    }

  private:
    void preparePrimsForRoi(GVA::RegionOfInterest &roi, std::vector<render::Prim> &prims) const;
    void preparePrimsForTensor(const GVA::Tensor &tensor, GVA::Rect<double> rect, std::vector<render::Prim> &prims,
                               size_t color_index = 0) const;
    void preparePrimsForKeypoints(const GVA::Tensor &tensor, GVA::Rect<double> rectangle,
                                  std::vector<render::Prim> &prims) const;
    void preparePrimsForKeypointConnections(GstStructure *s, const std::vector<float> &keypoints_data,
                                            const std::vector<uint32_t> &dims, const std::vector<float> &confidence,
                                            const GVA::Rect<double> &rectangle, std::vector<render::Prim> &prims) const;

    std::unique_ptr<Renderer> createRenderer(std::shared_ptr<ColorConverter> converter, DEVICE_SELECTOR device,
                                             InferenceBackend::MemoryType mem_type, dlstreamer::ContextPtr context);

    std::unique_ptr<Renderer> createGPURenderer(dlstreamer::ImageFormat format,
                                                std::shared_ptr<ColorConverter> converter,
                                                InferenceBackend::MemoryType mem_type,
                                                dlstreamer::ContextPtr vaapi_context);

    GstVideoInfo *_vinfo;
    std::string _backend_type;

    SharedObject::Ptr _gpurenderer_loader;
    std::unique_ptr<Renderer> _renderer;

    const int _thickness = 2;
    const double _radius_multiplier = 0.0025;
    const Color _default_color = indexToColor(1);
    // Position for full-frame text
    const cv::Point2f _ff_text_position = cv::Point2f(0, 25.f);
    struct FontCfg {
        const int type = cv::FONT_HERSHEY_TRIPLEX;
        const double scale = 1.0;
    } _font;
    const bool _obb = false;
};

enum { PROP_0, PROP_DEVICE, PROP_OBB };

G_DEFINE_TYPE_WITH_CODE(GstGvaWatermarkImpl, gst_gva_watermark_impl, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_watermark_impl_debug_category, "gvawatermarkimpl", 0,
                                                "debug category for gvawatermark element"));

static void gst_gva_watermark_impl_init(GstGvaWatermarkImpl *gvawatermark) {
    gvawatermark->device = DEFAULT_DEVICE;
    gvawatermark->obb = false;
}

void gst_gva_watermark_impl_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GstGvaWatermarkImpl *gvawatermark = GST_GVA_WATERMARK_IMPL(object);

    GST_DEBUG_OBJECT(gvawatermark, "set_property");

    switch (prop_id) {
    case PROP_DEVICE:
        g_free(gvawatermark->device);
        gvawatermark->device = g_value_dup_string(value);
        break;
    case PROP_OBB:
        gvawatermark->obb = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

void gst_gva_watermark_impl_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstGvaWatermarkImpl *self = GST_GVA_WATERMARK_IMPL(object);

    GST_DEBUG_OBJECT(self, "get_property");

    switch (prop_id) {
    case PROP_DEVICE:
        if (self->impl) {
            g_value_set_string(value, self->impl->getBackendType().c_str());
        } else {
            g_value_set_string(value, self->device);
        }
        break;
    case PROP_OBB:
        g_value_set_boolean(value, self->obb);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

void gst_gva_watermark_impl_dispose(GObject *object) {
    GstGvaWatermarkImpl *gvawatermark = GST_GVA_WATERMARK_IMPL(object);

    GST_DEBUG_OBJECT(gvawatermark, "dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gst_gva_watermark_impl_parent_class)->dispose(object);
}

void gst_gva_watermark_impl_finalize(GObject *object) {
    GstGvaWatermarkImpl *gvawatermark = GST_GVA_WATERMARK_IMPL(object);

    GST_DEBUG_OBJECT(gvawatermark, "finalize");

    gvawatermark->impl.reset();

    g_free(gvawatermark->device);
    gvawatermark->device = nullptr;

    G_OBJECT_CLASS(gst_gva_watermark_impl_parent_class)->finalize(object);
}

static gboolean gst_gva_watermark_impl_start(GstBaseTransform *trans) {
    GstGvaWatermarkImpl *gvawatermark = GST_GVA_WATERMARK_IMPL(trans);

    GST_DEBUG_OBJECT(gvawatermark, "start");

    GST_INFO_OBJECT(gvawatermark, "%s parameters:\n -- Device: %s\n", GST_ELEMENT_NAME(GST_ELEMENT_CAST(gvawatermark)),
                    gvawatermark->device);

    return true;
}

static gboolean gst_gva_watermark_impl_stop(GstBaseTransform *trans) {
    GstGvaWatermarkImpl *gvawatermark = GST_GVA_WATERMARK_IMPL(trans);

    GST_DEBUG_OBJECT(gvawatermark, "stop");

    return true;
}

static gboolean gst_gva_watermark_impl_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps) {
    using namespace InferenceBackend;
    UNUSED(outcaps);

    GstGvaWatermarkImpl *gvawatermark = GST_GVA_WATERMARK_IMPL(trans);

    GST_DEBUG_OBJECT(gvawatermark, "set_caps");

    gst_video_info_from_caps(&gvawatermark->info, incaps);
    const auto mem_type = memoryTypeFromCaps(incaps);
    DEVICE_SELECTOR device = DEVICE_CPU;
    if (!gvawatermark->device) {
        switch (mem_type) {
        case MemoryType::SYSTEM:
            device = DEVICE_CPU;
            break;
        case MemoryType::VAAPI:
        case MemoryType::DMA_BUFFER:
            device = DEVICE_GPU_AUTOSELECTED;
            break;
        default:
            GST_ERROR_OBJECT(gvawatermark, "Unsupported memory type: %d", static_cast<int>(mem_type));
            return false;
        }
    } else {
        if (std::string(gvawatermark->device) == "GPU") {
            device = DEVICE_GPU;
            if (get_caps_feature(incaps) == SYSTEM_MEMORY_CAPS_FEATURE) {
                GST_ELEMENT_ERROR(
                    gvawatermark, CORE, FAILED,
                    ("Device %s is incompatible with System Memory type."
                     " Please, set CPU device or use another type of memory in a pipeline (VASurface or DMABuf).",
                     gvawatermark->device),
                    (NULL));
                return false;
            }
        } else if (std::string(gvawatermark->device) == "CPU") {
            device = DEVICE_CPU;
        } else {
            GST_ELEMENT_ERROR(gvawatermark, CORE, FAILED, ("Unsupported 'device' property name"),
                              ("Device with %s name is not supported in the gvawatermark", gvawatermark->device));
            return false;
        }
    }

    gvawatermark->impl.reset();

    VaApiDisplayPtr va_dpy;
    if (mem_type == MemoryType::VAAPI) {
        try {
            va_dpy = std::make_shared<dlstreamer::GSTContextQuery>(
                trans, (get_caps_feature(incaps) == VA_MEMORY_CAPS_FEATURE) ? dlstreamer::MemoryType::VA
                                                                            : dlstreamer::MemoryType::VAAPI);
        } catch (const std::exception &e) {
            GST_ELEMENT_ERROR(gvawatermark, CORE, FAILED, ("Could not create VAAPI context"),
                              ("Cannot create watermark instance. %s", Utils::createNestedErrorMsg(e).c_str()));
        }
    }

    try {
        gvawatermark->impl = std::make_shared<Impl>(&gvawatermark->info, device, mem_type, va_dpy, gvawatermark->obb);
    } catch (const std::exception &e) {
        GST_ELEMENT_ERROR(gvawatermark, CORE, FAILED, ("Could not initialize"),
                          ("Cannot create watermark instance. %s", Utils::createNestedErrorMsg(e).c_str()));
    }

    if (!gvawatermark->impl)
        return false;

    GST_INFO_OBJECT(gvawatermark, "Watermark configuration:");
    GST_INFO_OBJECT(gvawatermark, "device: %s", gvawatermark->impl->getBackendType().c_str());

    return true;
}

static GstFlowReturn gst_gva_watermark_impl_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
    GstGvaWatermarkImpl *gvawatermark = GST_GVA_WATERMARK_IMPL(trans);

    GST_DEBUG_OBJECT(gvawatermark, "transform_ip");

    if (!gst_pad_is_linked(GST_BASE_TRANSFORM_SRC_PAD(trans))) {
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }

    // TODO: remove when problem with refcounting in inference elements is resolved
    if (!gst_buffer_is_writable(buf)) {
        GST_ELEMENT_WARNING(gvawatermark, STREAM, FAILED, ("Can't draw because buffer is not writable. Skipped"),
                            (nullptr));
        return GST_FLOW_OK;
    }

    try {
        if (!gvawatermark->impl)
            throw std::invalid_argument("Watermark is not set");
        gvawatermark->impl->render(buf);
    } catch (const std::exception &e) {
        const std::string msg = Utils::createNestedErrorMsg(e);
        GST_ELEMENT_ERROR(gvawatermark, STREAM, FAILED, ("gvawatermark has failed to process frame."),
                          ("%s", msg.c_str()));
        return GST_FLOW_ERROR;
    }

    return GST_FLOW_OK;
}

gboolean gst_gva_watermark_impl_propose_allocation(GstBaseTransform *trans, GstQuery *decide_query, GstQuery *query) {
    UNUSED(decide_query);
    UNUSED(trans);
    if (query) {
        gst_query_add_allocation_meta(query, GST_VIDEO_META_API_TYPE, nullptr);
        return true;
    }
    return false;
}

static void gst_gva_watermark_impl_class_init(GstGvaWatermarkImplClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    /* Setting up pads and setting metadata should be moved to
       base_class_init if you intend to subclass this class. */
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(WATERMARK_ALL_CAPS)));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(WATERMARK_ALL_CAPS)));

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");

    gobject_class->set_property = gst_gva_watermark_impl_set_property;
    gobject_class->get_property = gst_gva_watermark_impl_get_property;
    gobject_class->dispose = gst_gva_watermark_impl_dispose;
    gobject_class->finalize = gst_gva_watermark_impl_finalize;
    base_transform_class->start = GST_DEBUG_FUNCPTR(gst_gva_watermark_impl_start);
    base_transform_class->stop = GST_DEBUG_FUNCPTR(gst_gva_watermark_impl_stop);
    base_transform_class->set_caps = GST_DEBUG_FUNCPTR(gst_gva_watermark_impl_set_caps);
    base_transform_class->transform = nullptr;
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_gva_watermark_impl_transform_ip);
    base_transform_class->propose_allocation = GST_DEBUG_FUNCPTR(gst_gva_watermark_impl_propose_allocation);

    g_object_class_install_property(
        gobject_class, PROP_DEVICE,
        g_param_spec_string(
            "device", "Target device",
            "Supported devices are CPU and GPU. Default is CPU on system memory and GPU on video memory",
            DEFAULT_DEVICE, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(gobject_class, PROP_OBB,
                                    g_param_spec_boolean("obb", "Oriented Bounding Box",
                                                         "If true, draw oriented bounding box instead of object mask",
                                                         false,
                                                         (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

Impl::Impl(GstVideoInfo *info, DEVICE_SELECTOR device, InferenceBackend::MemoryType mem_type,
           dlstreamer::ContextPtr context, bool obb)
    : _vinfo(info), _obb(obb) {
    assert(_vinfo);
    if (GST_VIDEO_INFO_COLORIMETRY(_vinfo).matrix == GstVideoColorMatrix::GST_VIDEO_COLOR_MATRIX_UNKNOWN)
        throw std::runtime_error("GST_VIDEO_COLOR_MATRIX_UNKNOWN");

    double Kb = 0, Kr = 0;
    GstVideoColorimetry colorimetry = GST_VIDEO_INFO_COLORIMETRY(_vinfo);
    gst_video_color_matrix_get_Kr_Kb(colorimetry.matrix, &Kr, &Kb);

    dlstreamer::ImageFormat format = dlstreamer::gst_format_to_video_format(GST_VIDEO_INFO_FORMAT(_vinfo));
    std::shared_ptr<ColorConverter> converter = create_color_converter(format, color_table, Kr, Kb);

    _renderer = createRenderer(std::move(converter), device, mem_type, context);
}

size_t get_keypoint_index_by_name(const gchar *target_name, GValueArray *names) {
    if (names == nullptr or target_name == nullptr) {
        throw std::invalid_argument("get_keypoint_index_by_name: Got nullptrs.");
    }

    for (size_t i = 0; i < names->n_values; ++i) {
        const gchar *name = g_value_get_string(names->values + i);
        if (g_strcmp0(name, target_name) == 0) {
            return i;
        }
    }

    return names->n_values;
}

bool Impl::render(GstBuffer *buffer) {
    ITT_TASK(__FUNCTION__);

    GVA::VideoFrame video_frame(buffer, _vinfo);
    auto video_frame_rois = video_frame.regions();

    std::vector<render::Prim> prims;
    prims.reserve(video_frame_rois.size());
    // Prepare primitives for all ROIs
    for (auto &roi : video_frame_rois) {
        preparePrimsForRoi(roi, prims);
    }

    // Tensor metas, attached to the frame, should be related to full-frame inference
    const GVA::Rect<double> ff_rect{0, 0, safe_convert<double>(_vinfo->width), safe_convert<double>(_vinfo->height)};
    std::ostringstream ff_text;

    for (auto &tensor : video_frame.tensors()) {
        if (tensor.is_detection())
            continue;
        preparePrimsForTensor(tensor, ff_rect, prims);
        if (tensor.label().size() > 1) {
            appendStr(ff_text, tensor.label());
            ff_text << int(tensor.confidence() * 100) << "%";
        }
    }

    if (ff_text.tellp() != 0)
        prims.emplace_back(render::Text(ff_text.str(), _ff_text_position, _font.type, _font.scale, _default_color));

    // Skip render if there are no primitives to draw
    if (!prims.empty()) {
        auto gstbuffer = std::make_shared<dlstreamer::GSTFrame>(buffer, _vinfo);
        _renderer->draw(gstbuffer, prims);
    }

    return true;
}

void Impl::preparePrimsForRoi(GVA::RegionOfInterest &roi, std::vector<render::Prim> &prims) const {
    size_t color_index = roi.label_id();

    auto rect_u32 = roi.rect();
    GVA::Rect<double> rect = {safe_convert<double>(rect_u32.x), safe_convert<double>(rect_u32.y),
                              safe_convert<double>(rect_u32.w), safe_convert<double>(rect_u32.h)};

    // auto rect = roi.normalized_rect(_vinfo->width, _vinfo->height);
    // if (rect.w && rect.h) {
    //     rect.x *= _vinfo->width;
    //     rect.y *= _vinfo->height;
    //     rect.w *= _vinfo->width;
    //     rect.h *= _vinfo->height;
    // } else {
    //     auto rect_u32 = roi.rect();
    //     rect = {safe_convert<double>(rect_u32.x), safe_convert<double>(rect_u32.y), safe_convert<double>(rect_u32.w),
    //             safe_convert<double>(rect_u32.h)};
    // }
    clip_rect(rect.x, rect.y, rect.w, rect.h, _vinfo);

    std::ostringstream text;
    const int object_id = roi.object_id();
    if (object_id > 0) {
        text << object_id << ": ";
        color_index = object_id;
    }

    if (roi.label().size() > 1) {
        appendStr(text, roi.label());
        text << int(roi.confidence() * 100) << "%";
    }

    // Prepare primitives for tensors
    for (auto &tensor : roi.tensors()) {
        preparePrimsForTensor(tensor, rect, prims, color_index);
        if (!tensor.is_detection()) {
            appendStr(text, tensor.label());
        }
    }

    // put rectangle
    Color color = indexToColor(color_index);
    cv::Rect bbox_rect(rect.x, rect.y, rect.w, rect.h);
    if (!_obb)
        prims.emplace_back(render::Rect(bbox_rect, color, _thickness, roi.rotation()));

    // put text
    if (text.str().size() != 0) {
        cv::Point2f pos(rect.x, rect.y - 5.f);
        if (pos.y < 0)
            pos.y = rect.y + 30.f;
        prims.emplace_back(render::Text(text.str(), pos, _font.type, _font.scale, color));
    }
}

void Impl::preparePrimsForTensor(const GVA::Tensor &tensor, GVA::Rect<double> rect, std::vector<render::Prim> &prims,
                                 size_t color_index) const {
    // landmarks rendering
    if (tensor.model_name().find("landmarks") != std::string::npos || tensor.format() == "landmark_points") {
        std::vector<float> data = tensor.data<float>();
        for (size_t i = 0; i < data.size() / 2; i++) {
            Color color = indexToColor(i);
            int x_lm = safe_convert<int>(rect.x + rect.w * data[2 * i]);
            int y_lm = safe_convert<int>(rect.y + rect.h * data[2 * i + 1]);
            size_t radius = safe_convert<size_t>(1 + _radius_multiplier * rect.w);
            prims.emplace_back(render::Circle(cv::Point2i(x_lm, y_lm), radius, color, cv::FILLED));
        }
    }

    if (tensor.format() == "contour_points") {
        std::vector<float> data = tensor.data<float>();
        for (size_t i = 0; i < data.size(); i += 2) {
            int x = safe_convert<int>(rect.x + rect.w * data[i]);
            int y = safe_convert<int>(rect.y + rect.h * data[i + 1]);
            int x2, y2;
            if (i + 2 < data.size()) {
                x2 = safe_convert<int>(rect.x + rect.w * data[i + 2]);
                y2 = safe_convert<int>(rect.y + rect.h * data[i + 3]);
            } else {
                x2 = safe_convert<int>(rect.x + rect.w * data[0]);
                y2 = safe_convert<int>(rect.y + rect.h * data[1]);
            }
            prims.emplace_back(render::Line(cv::Point2i(x, y), cv::Point2i(x2, y2), _default_color, _thickness));
        }
    }

    if (tensor.format() == "segmentation_mask") {
        std::vector<float> mask = tensor.data<float>();
        std::vector<guint> dims = tensor.dims();
        assert(dims.size() == 2);
        const cv::Size &mask_size{int(dims[0]), int(dims[1])};
        cv::Rect2f box(rect.x, rect.y, rect.w, rect.h);
        Color color = indexToColor(color_index);

        if (!_obb) {
            // overlay mask on top of image pixels
            prims.emplace_back(render::InstanceSegmantationMask(mask, mask_size, color, box));
        } else {
            // resize mask to non-rotated bounding box and convert to binary
            cv::Mat mask_resized, mask_converted;
            cv::resize(cv::Mat{mask_size, CV_32F, mask.data()}, mask_resized, cv::Size(rect.w, rect.h));
            cv::threshold(mask_resized, mask_resized, 0.5f, 1.0f, cv::THRESH_BINARY);
            mask_resized.convertTo(mask_converted, CV_8UC1);
            // find contours in binary mask and derive minimal bounding box
            std::vector<std::vector<cv::Point>> contours;
            cv::findContours(mask_converted, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
            cv::RotatedRect rotated = cv::minAreaRect(contours[0]);
            // shift minimal rotated box to original box position and draw
            rotated.center = rotated.center + cv::Point2f(rect.x, rect.y);
            cv::Point2f vertices2f[4];
            rotated.points(vertices2f);
            for (int i = 0; i < 4; i++)
                prims.emplace_back(render::Line(vertices2f[i], vertices2f[(i + 1) % 4], color, _thickness));
        }
    }

    if (tensor.format() == "semantic_mask") {
        assert(tensor.precision() == GVA::Tensor::Precision::I64);
        std::vector<int64_t> mask = tensor.data<int64_t>();
        std::vector<guint> dims = tensor.dims();
        const cv::Size &mask_size{int(dims[1]), int(dims[2])};
        cv::Rect2f box(rect.x, rect.y, rect.w, rect.h);
        prims.emplace_back(render::SemanticSegmantationMask(mask, mask_size, box));
    }

    preparePrimsForKeypoints(tensor, rect, prims);
}

/**
 * Prepares primitives for key points and their conections using given tensor's info.
 */
void Impl::preparePrimsForKeypoints(const GVA::Tensor &tensor, GVA::Rect<double> rectangle,
                                    std::vector<render::Prim> &prims) const {
    if (tensor.format() != "keypoints")
        return;

    const auto keypoints_data = tensor.data<float>();
    const auto confidence = tensor.get_float_vector("confidence");

    if (keypoints_data.empty())
        throw std::runtime_error("Keypoints array is empty.");

    const auto &dimensions = tensor.dims();
    size_t points_num = dimensions[0];
    size_t point_dimension = dimensions[1];

    if (keypoints_data.size() != points_num * point_dimension)
        throw std::logic_error("The size of the keypoints data does not match the dimension: Size=" +
                               std::to_string(keypoints_data.size()) + " Dimension=[" + std::to_string(dimensions[0]) +
                               "," + std::to_string(dimensions[1]) + "].");

    for (size_t i = 0; i < points_num; ++i) {

        if ((confidence.size() > 0) && (confidence[i] < 0.5))
            continue;

        float x_real = keypoints_data[point_dimension * i];
        float y_real = keypoints_data[point_dimension * i + 1];

        if (x_real == -1.0f and y_real == -1.0f)
            continue;

        int x_lm = safe_convert<int>(rectangle.x + rectangle.w * x_real);
        int y_lm = safe_convert<int>(rectangle.y + rectangle.h * y_real);
        size_t radius = safe_convert<size_t>(1 + _radius_multiplier * (rectangle.w + rectangle.h));

        Color color = indexToColor(i);
        prims.emplace_back(render::Circle(cv::Point2i(x_lm, y_lm), radius, color, cv::FILLED));
    }

    preparePrimsForKeypointConnections(tensor.gst_structure(), keypoints_data, dimensions, confidence, rectangle,
                                       prims);
}

void Impl::preparePrimsForKeypointConnections(GstStructure *s, const std::vector<float> &keypoints_data,
                                              const std::vector<uint32_t> &dims, const std::vector<float> &confidence,
                                              const GVA::Rect<double> &rectangle,
                                              std::vector<render::Prim> &prims) const {
    if (not(gst_structure_has_field(s, "point_names") and gst_structure_has_field(s, "point_connections")))
        return;

    GValueArray *point_connections = nullptr;
    gst_structure_get_array(s, "point_connections", &point_connections);

    if (point_connections == nullptr)
        throw std::runtime_error("Arrays with point connections information is nullptr.");
    if (point_connections->n_values == 0)
        throw std::runtime_error("Arrays with point connections is empty.");

    GValueArray *point_names = nullptr;
    gst_structure_get_array(s, "point_names", &point_names);

    if (point_names == nullptr)
        throw std::runtime_error("Arrays with point names information is nullptr.");
    if (point_names->n_values == 0)
        throw std::runtime_error("Arrays with point names is empty.");

    size_t point_dimension = dims[1];
    if (point_names->n_values * point_dimension != keypoints_data.size())
        throw std::logic_error("Number of point names must be equal to number of keypoints.");

    if (point_connections->n_values % 2 != 0)
        throw std::logic_error("Expected even amount of point connections.");

    for (size_t i = 0; i < point_connections->n_values; i += 2) {
        const gchar *point_name_1 = g_value_get_string(point_connections->values + i);
        const gchar *point_name_2 = g_value_get_string(point_connections->values + i + 1);

        size_t index_1 = get_keypoint_index_by_name(point_name_1, point_names);
        size_t index_2 = get_keypoint_index_by_name(point_name_2, point_names);

        if (index_1 == point_names->n_values)
            throw std::runtime_error("Point name \"" + std::string(point_name_1) +
                                     "\" has not been found in point connections.");

        if (index_2 == point_names->n_values)
            throw std::runtime_error("Point name \"" + std::string(point_name_2) +
                                     "\" has not been found in point connections.");

        if (index_1 == index_2)
            throw std::logic_error("Point names in connection are the same: " + std::string(point_name_1) + " / " +
                                   std::string(point_name_2));

        if ((confidence.size() > 0) && ((confidence[index_1] < 0.5) || confidence[index_2] < 0.5))
            continue;

        index_1 = safe_mul(point_dimension, index_1);
        index_2 = safe_mul(point_dimension, index_2);

        float x1_real = keypoints_data[index_1];
        float y1_real = keypoints_data[index_1 + 1];
        float x2_real = keypoints_data[index_2];
        float y2_real = keypoints_data[index_2 + 1];

        if ((x1_real == -1.0f and y1_real == -1.0f) or (x2_real == -1.0f and y2_real == -1.0f))
            continue;

        int x1 = safe_convert<int>(rectangle.x + rectangle.w * x1_real);
        int y1 = safe_convert<int>(rectangle.y + rectangle.h * y1_real);
        int x2 = safe_convert<int>(rectangle.x + rectangle.w * x2_real);
        int y2 = safe_convert<int>(rectangle.y + rectangle.h * y2_real);

        prims.emplace_back(render::Line(cv::Point2i(x1, y1), cv::Point2i(x2, y2), _default_color, _thickness));
    }

    g_value_array_free(point_connections);
    g_value_array_free(point_names);
}

std::unique_ptr<Renderer> Impl::createRenderer(std::shared_ptr<ColorConverter> converter, DEVICE_SELECTOR device,
                                               InferenceBackend::MemoryType mem_type, dlstreamer::ContextPtr context) {

    dlstreamer::ImageFormat format = dlstreamer::gst_format_to_video_format(GST_VIDEO_INFO_FORMAT(_vinfo));
    if (device == DEVICE_GPU || device == DEVICE_GPU_AUTOSELECTED) {
        try {
            auto renderer = createGPURenderer(format, converter, mem_type, context);
            _backend_type = "GPU";
            return renderer;
        } catch (const std::exception &e) {
            if (device == DEVICE_GPU) {
                std::string err_msg =
                    "GPU Watermark initialization failed: " + std::string(e.what()) + ". " + Utils::dpcppInstructionMsg;
                throw std::runtime_error(err_msg);
            }
        }
    }
    _backend_type = "CPU";
    auto buf_mapper = BufferMapperFactory::createMapper(InferenceBackend::MemoryType::SYSTEM);
    return create_cpu_renderer(format, converter, std::move(buf_mapper));
}

std::unique_ptr<Renderer> Impl::createGPURenderer(dlstreamer::ImageFormat format,
                                                  std::shared_ptr<ColorConverter> converter,
                                                  InferenceBackend::MemoryType mem_type,
                                                  dlstreamer::ContextPtr vaapi_context) {

    constexpr char FUNCTION_NAME[] = "create_renderer";
    constexpr char LIBRARY_NAME[] = "libgpurenderer.so";

    auto dma_mapper = createMapperToDMA(mem_type, vaapi_context);

    using create_renderer_func_t = Renderer *(dlstreamer::ImageFormat format, std::shared_ptr<ColorConverter> converter,
                                              dlstreamer::MemoryMapperPtr input_buffer_mapper, int width, int height);

    _gpurenderer_loader = SharedObject::getLibrary(LIBRARY_NAME);
    auto create_renderer_func = _gpurenderer_loader->getFunction<create_renderer_func_t>(FUNCTION_NAME);

    auto renderer = create_renderer_func(format, converter, std::move(dma_mapper), GST_VIDEO_INFO_WIDTH(_vinfo),
                                         GST_VIDEO_INFO_HEIGHT(_vinfo));
    return std::unique_ptr<Renderer>(renderer);
}

static gboolean plugin_init(GstPlugin *plugin) {
    if (!gst_element_register(plugin, "gvawatermarkimpl", GST_RANK_NONE, GST_TYPE_GVA_WATERMARK_IMPL))
        return FALSE;

    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, gvawatermarkimpl, PRODUCT_FULL_NAME " gvawatermarkimpl element",
                  plugin_init, PLUGIN_VERSION, PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
