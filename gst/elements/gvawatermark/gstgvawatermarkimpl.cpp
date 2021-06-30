/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvawatermarkimpl.h"

#include <gst/allocators/gstdmabuf.h>
#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video-color.h>
#include <gst/video/video-info.h>
#include <inference_backend/logger.h>

#include "gva_caps.h"
#include "so_loader.h"
#include "utils.h"
#include "video_frame.h"

#include "renderer/color_converter.h"
#include "renderer/cpu/create_renderer.h"

#include <unistd.h>

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

static const std::vector<Color> color_table = {
    Color(255, 0, 0),   Color(0, 255, 0),   Color(0, 0, 255),   Color(255, 255, 0), Color(0, 255, 255),
    Color(255, 0, 255), Color(255, 170, 0), Color(255, 0, 170), Color(0, 255, 170), Color(170, 255, 0),
    Color(170, 0, 255), Color(0, 170, 255), Color(255, 85, 0),  Color(85, 255, 0),  Color(0, 255, 85),
    Color(0, 85, 255),  Color(85, 0, 255),  Color(255, 0, 85)};

static Color indexToColor(size_t index) {
    return color_table[index % color_table.size()];
}

static void clip_rect(double &x, double &y, double &w, double &h, GstVideoInfo *info) {
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

} // namespace

struct Impl {
    Impl(GstVideoInfo *info, DEVICE_SELECTOR device);
    bool render(GstBuffer *buffer);
    const std::string &getBackendType() const {
        return _backend_type;
    }

  private:
    void preparePrimsForRoi(GVA::RegionOfInterest &roi, std::vector<gapidraw::Prim> &prims) const;
    void preparePrimsForTensor(const GVA::Tensor &tensor, GVA::Rect<double> rect,
                               std::vector<gapidraw::Prim> &prims) const;
    void preparePrimsForKeypoints(const GVA::Tensor &tensor, GVA::Rect<double> rectangle,
                                  std::vector<gapidraw::Prim> &prims) const;
    void preparePrimsForKeypointConnections(GstStructure *s, const std::vector<float> &keypoints_data,
                                            const std::vector<uint32_t> &dims, const GVA::Rect<double> &rectangle,
                                            std::vector<gapidraw::Prim> &prims) const;

    std::unique_ptr<Renderer> createRenderer(const std::vector<Color> &rgb_color_table, double Kr, double Kb,
                                             DEVICE_SELECTOR device);

    std::unique_ptr<Renderer> createGPURenderer(InferenceBackend::FourCC format,
                                                std::shared_ptr<ColorConverter> converter);

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
};

enum { PROP_0, PROP_DEVICE };

G_DEFINE_TYPE_WITH_CODE(GstGvaWatermarkImpl, gst_gva_watermark_impl, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_watermark_impl_debug_category, "gvawatermarkimpl", 0,
                                                "debug category for gvawatermark element"));

static void gst_gva_watermark_impl_init(GstGvaWatermarkImpl *gvawatermark) {
    gvawatermark->device = DEFAULT_DEVICE;
}

void gst_gva_watermark_impl_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GstGvaWatermarkImpl *gvawatermark = GST_GVA_WATERMARK_IMPL(object);

    GST_DEBUG_OBJECT(gvawatermark, "set_property");

    switch (prop_id) {
    case PROP_DEVICE:
        g_free(gvawatermark->device);
        gvawatermark->device = g_value_dup_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

void gst_gva_watermark_impl_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstGvaWatermarkImpl *gvawatermark = GST_GVA_WATERMARK_IMPL(object);

    GST_DEBUG_OBJECT(gvawatermark, "get_property");

    switch (prop_id) {
    case PROP_DEVICE:
        g_value_set_string(value, gvawatermark->device);
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

    if (gvawatermark->impl)
        delete gvawatermark->impl;

    g_free(gvawatermark->device);
    gvawatermark->device = nullptr;

    G_OBJECT_CLASS(gst_gva_watermark_impl_parent_class)->finalize(object);
}

static gboolean gst_gva_watermark_impl_start(GstBaseTransform *trans) {
    GstGvaWatermarkImpl *gvawatermark = GST_GVA_WATERMARK_IMPL(trans);

    GST_DEBUG_OBJECT(gvawatermark, "start");
    return true;
}

static gboolean gst_gva_watermark_impl_stop(GstBaseTransform *trans) {
    GstGvaWatermarkImpl *gvawatermark = GST_GVA_WATERMARK_IMPL(trans);

    GST_DEBUG_OBJECT(gvawatermark, "stop");

    return true;
}

static gboolean gst_gva_watermark_impl_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps) {
    UNUSED(outcaps);

    GstGvaWatermarkImpl *gvawatermark = GST_GVA_WATERMARK_IMPL(trans);

    GST_DEBUG_OBJECT(gvawatermark, "set_caps");

    gst_video_info_from_caps(&gvawatermark->info, incaps);
    DEVICE_SELECTOR device = DEVICE_CPU;
    if (!gvawatermark->device) {
        switch (get_caps_feature(incaps)) {
        case SYSTEM_MEMORY_CAPS_FEATURE:
            device = DEVICE_CPU;
            gvawatermark->device = g_strdup("CPU");
            break;
        case VA_SURFACE_CAPS_FEATURE:
        case DMA_BUF_CAPS_FEATURE:
            device = DEVICE_GPU_AUTOSELECTED;
            gvawatermark->device = g_strdup("GPU");
            break;
        default:
            GST_ERROR_OBJECT(gvawatermark, "Unknown caps property %s", gst_caps_to_string(incaps));
            return false;
        }
    } else {
        if (std::string(gvawatermark->device) == "GPU") {
            device = DEVICE_GPU;
        } else if (std::string(gvawatermark->device) == "CPU") {
            device = DEVICE_CPU;
        } else {
            GST_ELEMENT_ERROR(gvawatermark, CORE, FAILED, ("Unsupported 'device' property name"),
                              ("Device with %s name is not supported in the gvawatermark", gvawatermark->device));
            return false;
        }
    }

    if (gvawatermark->impl) {
        delete gvawatermark->impl;
        gvawatermark->impl = nullptr;
    }

    try {
        gvawatermark->impl = new Impl(&gvawatermark->info, device);
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
        GST_ERROR("Cannot draw primitives. %s", Utils::createNestedErrorMsg(e).c_str());
        GST_ELEMENT_ERROR(gvawatermark, STREAM, FAILED, ("gvawatermark has failed to process frame."),
                          ("gvawatermark has failed to process frame"));
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
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));

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
}

Impl::Impl(GstVideoInfo *info, DEVICE_SELECTOR device) : _vinfo(info) {
    assert(_vinfo);
    if (GST_VIDEO_INFO_COLORIMETRY(_vinfo).matrix == GstVideoColorMatrix::GST_VIDEO_COLOR_MATRIX_UNKNOWN)
        throw std::runtime_error("GST_VIDEO_COLOR_MATRIX_UNKNOWN");

    double Kb = 0, Kr = 0;
    GstVideoColorimetry colorimetry = GST_VIDEO_INFO_COLORIMETRY(_vinfo);
    gst_video_color_matrix_get_Kr_Kb(colorimetry.matrix, &Kr, &Kb);

    _renderer = createRenderer(color_table, Kr, Kb, device);
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
    std::vector<gapidraw::Prim> prims;
    prims.reserve(video_frame.regions().size());
    // Prepare primitives for all ROIs
    for (auto &roi : video_frame.regions()) {
        preparePrimsForRoi(roi, prims);
    }

    // Tensor metas, attached to the frame, should be related to full-frame inference
    const GVA::Rect<double> ff_rect{0, 0, static_cast<double>(_vinfo->width), static_cast<double>(_vinfo->height)};
    std::ostringstream ff_text;

    for (auto &tensor : video_frame.tensors()) {
        assert(!tensor.is_detection());
        preparePrimsForTensor(tensor, ff_rect, prims);
        appendStr(ff_text, tensor.label());
    }

    if (ff_text.tellp() != 0)
        prims.emplace_back(gapidraw::Text(ff_text.str(), _ff_text_position, _font.type, _font.scale, _default_color));

    _renderer->draw(buffer, _vinfo, prims);

    return true;
}

void Impl::preparePrimsForRoi(GVA::RegionOfInterest &roi, std::vector<gapidraw::Prim> &prims) const {
    size_t color_index = roi.label_id();

    auto rect = roi.normalized_rect();
    if (rect.w && rect.h) {
        rect.x *= _vinfo->width;
        rect.y *= _vinfo->height;
        rect.w *= _vinfo->width;
        rect.h *= _vinfo->height;
    } else {
        auto rect_u32 = roi.rect();
        rect = {(double)rect_u32.x, (double)rect_u32.y, (double)rect_u32.w, (double)rect_u32.h};
    }
    clip_rect(rect.x, rect.y, rect.w, rect.h, _vinfo);

    std::ostringstream text;
    const int object_id = roi.object_id();
    if (object_id > 0) {
        text << object_id << ": ";
        color_index = object_id;
    }

    appendStr(text, roi.label());

    // Prepare primitives for tensors
    for (auto &tensor : roi.tensors()) {
        preparePrimsForTensor(tensor, rect, prims);
        if (!tensor.is_detection())
            appendStr(text, tensor.label());
    }

    // put rectangle
    Color color = indexToColor(color_index);
    cv::Rect bbox_rect(rect.x, rect.y, rect.w, rect.h);
    prims.emplace_back(gapidraw::Rect(bbox_rect, color, _thickness));

    // put text
    cv::Point2f pos(rect.x, rect.y - 5.f);
    if (pos.y < 0)
        pos.y = rect.y + 30.f;
    prims.emplace_back(gapidraw::Text(text.str(), pos, _font.type, _font.scale, color));
}

void Impl::preparePrimsForTensor(const GVA::Tensor &tensor, GVA::Rect<double> rect,
                                 std::vector<gapidraw::Prim> &prims) const {
    // landmarks rendering
    if (tensor.model_name().find("landmarks") != std::string::npos || tensor.format() == "landmark_points") {
        std::vector<float> data = tensor.data<float>();
        for (size_t i = 0; i < data.size() / 2; i++) {
            Color color = indexToColor(i);
            int x_lm = rect.x + rect.w * data[2 * i];
            int y_lm = rect.y + rect.h * data[2 * i + 1];
            size_t radius = 1 + static_cast<int>(_radius_multiplier * rect.w);
            prims.emplace_back(gapidraw::Circle(cv::Point2i(x_lm, y_lm), radius, color, cv::FILLED));
        }
    }

    preparePrimsForKeypoints(tensor, rect, prims);
}

/**
 * Prepares primitives for key points and their conections using given tensor's info.
 */
void Impl::preparePrimsForKeypoints(const GVA::Tensor &tensor, GVA::Rect<double> rectangle,
                                    std::vector<gapidraw::Prim> &prims) const {
    if (tensor.format() != "keypoints")
        return;

    const auto keypoints_data = tensor.data<float>();

    if (keypoints_data.empty())
        throw std::runtime_error("Keypoints array is empty.");

    const auto &dimention = tensor.dims();
    size_t points_num = dimention[0];
    size_t point_dimension = dimention[1];

    if (keypoints_data.size() != points_num * point_dimension)
        throw std::logic_error("The size of the keypoints data does not match the dimension: Size=" +
                               std::to_string(keypoints_data.size()) + " Dimension=[" + std::to_string(dimention[0]) +
                               "," + std::to_string(dimention[1]) + "].");

    for (size_t i = 0; i < points_num; ++i) {
        float x_real = keypoints_data[point_dimension * i];
        float y_real = keypoints_data[point_dimension * i + 1];

        if (x_real == -1.0f and y_real == -1.0f)
            continue;

        int x_lm = rectangle.x + rectangle.w * x_real;
        int y_lm = rectangle.y + rectangle.h * y_real;
        size_t radius = 1 + static_cast<int>(_radius_multiplier * (rectangle.w + rectangle.h));

        Color color = indexToColor(i);
        prims.emplace_back(gapidraw::Circle(cv::Point2i(x_lm, y_lm), radius, color, cv::FILLED));
    }

    preparePrimsForKeypointConnections(tensor.gst_structure(), keypoints_data, dimention, rectangle, prims);
}

void Impl::preparePrimsForKeypointConnections(GstStructure *s, const std::vector<float> &keypoints_data,
                                              const std::vector<uint32_t> &dims, const GVA::Rect<double> &rectangle,
                                              std::vector<gapidraw::Prim> &prims) const {
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
        throw std::logic_error("Number of point names must ve equal to number of keypoints.");

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

        index_1 *= point_dimension;
        index_2 *= point_dimension;

        float x1_real = keypoints_data[index_1];
        float y1_real = keypoints_data[index_1 + 1];
        float x2_real = keypoints_data[index_2];
        float y2_real = keypoints_data[index_2 + 1];

        if ((x1_real == -1.0f and y1_real == -1.0f) or (x2_real == -1.0f and y2_real == -1.0f))
            continue;

        int x1 = rectangle.x + rectangle.w * x1_real;
        int y1 = rectangle.y + rectangle.h * y1_real;
        int x2 = rectangle.x + rectangle.w * x2_real;
        int y2 = rectangle.y + rectangle.h * y2_real;

        prims.emplace_back(gapidraw::Line(cv::Point2i(x1, y1), cv::Point2i(x2, y2), _default_color, _thickness));
    }

    g_value_array_free(point_connections);
    g_value_array_free(point_names);
}

std::unique_ptr<Renderer> Impl::createRenderer(const std::vector<Color> &rgb_color_table, double Kr, double Kb,
                                               DEVICE_SELECTOR device) {

    InferenceBackend::FourCC format =
        static_cast<InferenceBackend::FourCC>(gst_format_to_fourcc(GST_VIDEO_INFO_FORMAT(_vinfo)));
    std::shared_ptr<ColorConverter> converter = create_color_converter(format, rgb_color_table, Kr, Kb);
    if (device == DEVICE_GPU || device == DEVICE_GPU_AUTOSELECTED) {
        try {
            auto renderer = createGPURenderer(format, converter);
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
    return create_cpu_renderer(format, converter, InferenceBackend::MemoryType::SYSTEM);
}

std::unique_ptr<Renderer> Impl::createGPURenderer(InferenceBackend::FourCC format,
                                                  std::shared_ptr<ColorConverter> converter) {

    constexpr char FUNCTION_NAME[] =
        "_Z15create_rendererN16InferenceBackend6FourCCESt10shared_ptrI14ColorConverterENS_10MemoryTypeEii";
    constexpr char LIBRARY_NAME[] = "libgpurenderer.so";

    using create_renderer_func_t =
        std::unique_ptr<Renderer>(InferenceBackend::FourCC format, std::shared_ptr<ColorConverter> converter,
                                  InferenceBackend::MemoryType memory_type, int width, int height);

    _gpurenderer_loader = SharedObject::getLibrary(LIBRARY_NAME);
    auto create_renderer_func = _gpurenderer_loader->getFunction<create_renderer_func_t>(FUNCTION_NAME);

    return create_renderer_func(format, converter, InferenceBackend::MemoryType::USM_DEVICE_POINTER, _vinfo->width,
                                _vinfo->height);
}
