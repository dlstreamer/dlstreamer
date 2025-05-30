/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/element.h"
#include "dlstreamer/frame.h"
#include "dlstreamer/utils.h"
#include <gst/video/gstvideometa.h>
#include <limits>

namespace dlstreamer {

#define DLS_TENSOR_MEDIA_NAME "other/tensors"

static inline ImageFormat gst_format_to_video_format(int format) {
    switch (format) {
    case GST_VIDEO_FORMAT_BGR:
        return ImageFormat::BGR;
    case GST_VIDEO_FORMAT_RGB:
        return ImageFormat::RGB;
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_BGRx:
        return ImageFormat::BGRX;
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_RGBx:
        return ImageFormat::RGBX;
    case GST_VIDEO_FORMAT_GBR:
        return ImageFormat::RGBP;
#if (GST_VERSION_MAJOR >= 1) && (GST_VERSION_MINOR >= 20)
    case GST_VIDEO_FORMAT_RGBP:
        return ImageFormat::RGBP;
    case GST_VIDEO_FORMAT_BGRP:
        return ImageFormat::BGRP;
#endif
    case GST_VIDEO_FORMAT_NV12:
        return ImageFormat::NV12;
    case GST_VIDEO_FORMAT_I420:
        return ImageFormat::I420;
    default:
        throw std::runtime_error("Unsupported GST_VIDEO_FORMAT: " + std::to_string(format));
    }
}

static inline GstVideoFormat video_format_to_gst_format(ImageFormat format) {
    switch (format) {
    case ImageFormat::BGR:
        return GST_VIDEO_FORMAT_BGR;
    case ImageFormat::RGB:
        return GST_VIDEO_FORMAT_RGB;
    case ImageFormat::BGRX:
        return GST_VIDEO_FORMAT_BGRA;
    case ImageFormat::RGBX:
        return GST_VIDEO_FORMAT_RGBA;
#if (GST_VERSION_MAJOR >= 1) && (GST_VERSION_MINOR >= 20)
    case ImageFormat::RGBP:
        return GST_VIDEO_FORMAT_RGBP;
    case ImageFormat::BGRP:
        return GST_VIDEO_FORMAT_BGRP;
#else
    case ImageFormat::RGBP:
        return GST_VIDEO_FORMAT_GBR;
    case ImageFormat::BGRP:
        break; // not supported
#endif
    case ImageFormat::NV12:
        return GST_VIDEO_FORMAT_NV12;
    case ImageFormat::I420:
        return GST_VIDEO_FORMAT_I420;
    }
    throw std::runtime_error("Unsupported ImageFormat: " + image_format_to_string(format));
}

static inline FrameInfo gst_video_info_to_frame_info(const GstVideoInfo *vinfo) {
    if (!vinfo)
        throw std::runtime_error("video info is NULL");

    FrameInfo info;
    info.media_type = MediaType::Image;
    info.memory_type = MemoryType::CPU;

    if (vinfo->finfo) {
        info.format = int(gst_format_to_video_format(GST_VIDEO_INFO_FORMAT(vinfo)));
        const guint n_planes = GST_VIDEO_INFO_N_PLANES(vinfo);
        for (guint i = 0; i < n_planes; i++) {
            size_t width = GST_VIDEO_INFO_COMP_WIDTH(vinfo, i);
            size_t height = GST_VIDEO_INFO_COMP_HEIGHT(vinfo, i);
            size_t stride = GST_VIDEO_INFO_PLANE_STRIDE(vinfo, i);
            size_t channels = GST_VIDEO_INFO_COMP_PSTRIDE(vinfo, i);

            if (width && height && channels) {
                TensorInfo plane({height, width, channels}, DataType::UInt8);
                if (stride)
                    plane.stride = {stride, channels, sizeof(uint8_t)};
                info.tensors.push_back(plane);
            }
        }

        // planar formats with several identical tensors (ex, RGBP)
        if (info.tensors.size() > 1) {
            bool identical = true;
            const TensorInfo &plane0 = info.tensors[0];
            for (guint i = 1; i < n_planes; i++) {
                const TensorInfo &planei = info.tensors[i];
                if (planei.shape != plane0.shape || planei.stride != plane0.stride) {
                    identical = false;
                    break;
                }
            }
            if (identical && ImageInfo(info.tensors[0]).channels() == 1) {
                size_t width = GST_VIDEO_INFO_COMP_WIDTH(vinfo, 0);
                size_t height = GST_VIDEO_INFO_COMP_HEIGHT(vinfo, 0);
                size_t stride = GST_VIDEO_INFO_PLANE_STRIDE(vinfo, 0);
                size_t offset = GST_VIDEO_INFO_PLANE_OFFSET(vinfo, 1); // offset of second plane

                TensorInfo plane({n_planes, height, width}, DataType::UInt8);
                if (stride && offset)
                    plane.stride = {offset, stride, sizeof(uint8_t)};
                info.tensors = {plane};
            }
        }
    }

    return info;
}

static inline GstCapsFeatures *memory_type_to_gst_caps_feature(MemoryType memory_type) {
    GstCapsFeatures *features = nullptr;
    switch (memory_type) {
    case MemoryType::Any:
    case MemoryType::GST:
        features = gst_caps_features_new_any();
        break;
    case MemoryType::CPU:
        features = gst_caps_features_new_empty();
        break;
    default: {
        std::string str = std::string("memory:").append(memory_type_to_string(memory_type));
        features = gst_caps_features_from_string(str.c_str());
    } break;
    }
    return features;
}

namespace detail {

static std::string_view GST_VIDEO_MEDIA_NAME = "video/x-raw";

inline FrameInfo gst_video_caps_to_frame_info(const GstCaps *caps, guint index) {
    GstStructure *gst_structure = gst_caps_get_structure(caps, index);
    assert(GST_VIDEO_MEDIA_NAME == gst_structure_get_name(gst_structure));
    GstVideoInfo video_info = {};
    const gchar *format_str = gst_structure_get_string(gst_structure, "format");
    if (!format_str || !gst_caps_is_fixed(caps) || !gst_video_info_from_caps(&video_info, caps)) {
        gst_structure_get_int(gst_structure, "width", &video_info.width);
        gst_structure_get_int(gst_structure, "height", &video_info.height);
        if (format_str)
            video_info.finfo = gst_video_format_get_info(gst_video_format_from_string(format_str));
    }
    return gst_video_info_to_frame_info(&video_info);
}

inline FrameInfo gst_tensor_caps_to_frame_info(const GstCaps *caps, guint index) {
    FrameInfo info;
    info.media_type = MediaType::Tensors;

    GstStructure *gst_structure = gst_caps_get_structure(caps, index);
    guint num_tensors = 0;
    gst_structure_get_uint(gst_structure, "num_tensors", &num_tensors);
    if (!num_tensors)
        return info;

    auto types_str = gst_structure_get_string(gst_structure, "types");
    auto shapes_str = gst_structure_get_string(gst_structure, "dimensions");
    auto strides_str = gst_structure_get_string(gst_structure, "strides");
    if (!types_str)
        throw std::invalid_argument("Tensor type not specified in caps structure");
    if (!shapes_str)
        shapes_str = "";
    if (!strides_str)
        strides_str = "";
    auto types_array = split_string(types_str, ',');
    auto shapes_array = split_string(shapes_str, ',');
    auto strides_array = split_string(strides_str, ',');

    for (size_t i = 0; i < num_tensors; i++) {
        DataType type = datatype_from_string(types_array[i]);
        auto shape = shapes_array.size() ? shape_from_string(shapes_array[i]) : std::vector<size_t>();
        auto stride = (i < strides_array.size()) ? shape_from_string(strides_array[i]) : std::vector<size_t>();

        // reverse order
        std::reverse(shape.begin(), shape.end());
        std::reverse(stride.begin(), stride.end());

        info.tensors.push_back(TensorInfo(std::move(shape), type, stride));
    }
    return info;
}

inline GstStructure *frame_info_to_gst_video_caps(const FrameInfo &info) {
    GstStructure *structure = gst_structure_new_empty("video/x-raw");
    if (info.format) {
        GstVideoFormat gst_format = video_format_to_gst_format(static_cast<ImageFormat>(info.format));
        gst_structure_set(structure, "format", G_TYPE_STRING, gst_video_format_to_string(gst_format), NULL);
    }
    if (!info.tensors.empty()) {
        ImageInfo image_info(info.tensors[0]);
        if (image_info.layout() != ImageLayout::Any) {
            gst_structure_set(structure, "width", G_TYPE_INT, image_info.width(), NULL);
            gst_structure_set(structure, "height", G_TYPE_INT, image_info.height(), NULL);
        }
    }

    return structure;
}

inline GstStructure *frame_info_to_gst_tensor_caps(const FrameInfo &info) {
    GstStructure *structure = gst_structure_new_empty(DLS_TENSOR_MEDIA_NAME);
    size_t num_tensors = info.tensors.size();
    if (!num_tensors)
        return structure;

    std::ostringstream precision_str, dims_str, strides_str;
    bool is_strides_contiguous = true;

    for (size_t i = 0; i < num_tensors; i++) {
        if (i) {
            precision_str << ",";
            dims_str << ",";
            strides_str << ",";
        }
        precision_str << datatype_to_string(info.tensors[i].dtype);
        // dims and strides in reverse order
        dims_str << join_strings(info.tensors[i].shape.rbegin(), info.tensors[i].shape.rend(), ':');
        strides_str << join_strings(info.tensors[i].stride.rbegin(), info.tensors[i].stride.rend(), ':');
        is_strides_contiguous = is_strides_contiguous && info.tensors[i].is_contiguous();
    }

    gst_structure_set(structure, "num_tensors", G_TYPE_UINT, num_tensors, NULL);
    if (!precision_str.str().empty())
        gst_structure_set(structure, "types", G_TYPE_STRING, precision_str.str().c_str(), NULL);
    if (!dims_str.str().empty())
        gst_structure_set(structure, "dimensions", G_TYPE_STRING, dims_str.str().c_str(), NULL);
    if (!is_strides_contiguous)
        gst_structure_set(structure, "strides", G_TYPE_STRING, strides_str.str().c_str(), NULL);

    return structure;
}

} // namespace detail

static inline GstCaps *frame_info_to_gst_caps(const FrameInfo &info) {
    GstStructure *structure = nullptr;
    if (info.media_type == MediaType::Any) {
        return gst_caps_new_any();
    } else if (info.media_type == MediaType::Image) {
        structure = detail::frame_info_to_gst_video_caps(info);
    } else if (info.media_type == MediaType::Tensors) {
        structure = detail::frame_info_to_gst_tensor_caps(info);
    } else {
        throw std::runtime_error("Unsupported MediaType");
    }

    GstCaps *caps = gst_caps_new_full(structure, nullptr);
    gst_caps_set_features(caps, 0, memory_type_to_gst_caps_feature(info.memory_type));
    return caps;
}

static inline GstCaps *frame_info_vector_to_gst_caps(const FrameInfoVector &infos) {
    if (infos.empty())
        return gst_caps_new_any();
    GstCaps *result = gst_caps_new_empty();
    for (auto &&info : infos) {
        try {
            auto caps = frame_info_to_gst_caps(info);
            gst_caps_append(result, caps);
        } catch (const std::exception &e) {
            GST_ERROR("Error on frame_info_to_gst_caps: %s", e.what());
        }
    }
    return result;
}

static inline FrameInfo gst_caps_to_frame_info(const GstCaps *caps, guint index = 0) {
    GstStructure *structure = gst_caps_get_structure(caps, index);
    const std::string_view media_type = gst_structure_get_name(structure);

    FrameInfo info;
    if (media_type == detail::GST_VIDEO_MEDIA_NAME) {
        info = detail::gst_video_caps_to_frame_info(caps, index);
    } else if (media_type == DLS_TENSOR_MEDIA_NAME) {
        info = detail::gst_tensor_caps_to_frame_info(caps, index);
    } else {
        throw std::runtime_error(std::string("Unsupported media type ").append(media_type));
    }

    std::string feature = gst_caps_features_to_string(gst_caps_get_features(caps, index));
    if (feature.rfind("memory:", 0) == 0)
        info.memory_type = memory_type_from_string(feature.substr(7));
    else
        info.memory_type = MemoryType::CPU;
    return info;
}

static inline AccessMode gst_map_flags_to_access_mode(GstMapFlags flags) {
    int mode = 0;
    if (flags & GST_MAP_READ)
        mode |= static_cast<int>(AccessMode::Read);
    if (flags & GST_MAP_WRITE)
        mode |= static_cast<int>(AccessMode::Write);
    return static_cast<AccessMode>(mode);
}

static inline std::optional<Any> gvalue_to_any(const GValue *gval, const ParamDesc *desc = nullptr) noexcept;
static inline void any_to_gvalue(Any value, GValue *gvalue, bool init = true, const ParamDesc *desc = nullptr) noexcept;

template <typename T>
static inline std::vector<T> gvalue_to_vector(const GValue *value) noexcept {
    try {
        std::vector<T> vec(gst_value_array_get_size(value));
        for (size_t i = 0; i < vec.size(); i++)
            vec[i] = any_cast<T>(*gvalue_to_any(gst_value_array_get_value(value, i)));
        return vec;
    } catch (const std::bad_variant_access &e) {
        GST_ERROR("Bad variant access in gvalue_to_vector: %s", e.what());
        return std::vector<T>();
    } catch (...) {
        GST_ERROR("Unknown exception occurred in gvalue_to_vector");
        return std::vector<T>();
    }
}

static inline std::optional<Any> gvalue_to_any(const GValue *gval, const ParamDesc *desc) noexcept {
    if (!gval)
        return {};
    if (G_VALUE_TYPE(gval) == GST_TYPE_ARRAY) {
        switch (G_VALUE_TYPE(gst_value_array_get_value(gval, 0))) { // type of first element
        case G_TYPE_INT:
            return gvalue_to_vector<int>(gval);
        case G_TYPE_UINT:
            return gvalue_to_vector<size_t>(gval);
        case G_TYPE_DOUBLE:
            return gvalue_to_vector<double>(gval);
        case G_TYPE_STRING:
            return gvalue_to_vector<std::string>(gval);
        }
        return {};
    } else if (G_VALUE_TYPE(gval) == GST_TYPE_FRACTION) {
        return std::pair<int, int>(gst_value_get_fraction_numerator(gval), gst_value_get_fraction_denominator(gval));
    } else {
        switch (G_VALUE_TYPE(gval)) {
        case G_TYPE_INT:
            return g_value_get_int(gval);
        case G_TYPE_UINT:
            return static_cast<size_t>(g_value_get_uint(gval));
        case G_TYPE_DOUBLE:
            return g_value_get_double(gval);
        case G_TYPE_BOOLEAN:
            return static_cast<bool>(g_value_get_boolean(gval));
        case G_TYPE_STRING:
            return std::string(g_value_get_string(gval));
        case G_TYPE_ENUM:
            return desc ? desc->range[g_value_get_enum(gval)] : std::optional<Any>();
        case G_TYPE_POINTER:
            return reinterpret_cast<intptr_t>(g_value_get_pointer(gval));
        default:
            if (g_type_is_a(G_VALUE_TYPE(gval), G_TYPE_ENUM)) {
                return desc ? desc->range[g_value_get_enum(gval)] : std::optional<Any>();
            }
            GST_DEBUG("Unknown gtype: %s", g_type_name(G_VALUE_TYPE(gval)));
            return {};
        }
    }
}

template <typename T>
static void vector_to_gvalue(std::vector<T> vec, GValue *gvalue) {
    for (const T &elem : vec) {
        GValue gelem = G_VALUE_INIT;
        any_to_gvalue(Any(elem), &gelem);
        gst_value_array_append_value(gvalue, &gelem);
        g_value_unset(&gelem);
    }
}

static inline void any_to_gvalue(Any value, GValue *gvalue, bool init, const ParamDesc *desc) noexcept {
    if (any_holds_type<int>(value)) {
        if (init)
            g_value_init(gvalue, G_TYPE_INT);
        g_value_set_int(gvalue, any_cast<int>(value));
    } else if (any_holds_type<size_t>(value)) {
        if (init)
            g_value_init(gvalue, G_TYPE_UINT);
        g_value_set_uint(gvalue, any_cast<size_t>(value));
    } else if (any_holds_type<double>(value)) {
        if (init)
            g_value_init(gvalue, G_TYPE_DOUBLE);
        g_value_set_double(gvalue, any_cast<double>(value));
    } else if (any_holds_type<bool>(value)) {
        if (init)
            g_value_init(gvalue, G_TYPE_BOOLEAN);
        g_value_set_boolean(gvalue, any_cast<bool>(value));
    } else if (any_holds_type<std::string>(value)) {
        if (desc && !desc->range.empty()) {
            if (init)
                g_value_init(gvalue, G_TYPE_ENUM);
            size_t index = desc->range.size();
            try {
                // This code shouldn't be throwing anything (comparisons fail if values are incompatible).
                auto it = std::find(desc->range.begin(), desc->range.end(), value);
                if (it == desc->range.end()) {
                    GST_ERROR("Unknown enum name %s. Valid names are:", any_cast<std::string>(value).c_str());
                    for (auto &a : desc->range) {
                        GST_ERROR("\t%s", any_cast<std::string>(a).c_str());
                    }
                }
                index = it - desc->range.begin();
            } catch (const std::bad_variant_access &e) {
                GST_ERROR("Bad variant access: %s", e.what());
            } catch (...) {
                GST_ERROR("Unknown exception occurred");
            }
            g_value_set_enum(gvalue, index);
        } else {
            if (init)
                g_value_init(gvalue, G_TYPE_STRING);
            g_value_set_string(gvalue, any_cast<std::string>(value).c_str());
        }
    } else if (any_holds_type<intptr_t>(value)) {
        if (init)
            g_value_init(gvalue, G_TYPE_POINTER);
        g_value_set_pointer(gvalue, reinterpret_cast<void *>(any_cast<intptr_t>(value)));
    } else if (any_holds_type<std::vector<double>>(value)) {
        if (init)
            g_value_init(gvalue, GST_TYPE_ARRAY);
        vector_to_gvalue(any_cast<std::vector<double>>(value), gvalue);
    } else if (any_holds_type<std::vector<size_t>>(value)) {
        if (init)
            g_value_init(gvalue, GST_TYPE_ARRAY);
        vector_to_gvalue(any_cast<std::vector<size_t>>(value), gvalue);
    } else if (any_holds_type<std::vector<std::string>>(value)) {
        if (init)
            g_value_init(gvalue, GST_TYPE_ARRAY);
        vector_to_gvalue(any_cast<std::vector<std::string>>(value), gvalue);
    } else if (any_holds_type<std::pair<int, int>>(value)) {
        if (init)
            g_value_init(gvalue, GST_TYPE_FRACTION);
        std::pair<int, int> fraction{0, 1};
        try {
            fraction = any_cast<std::pair<int, int>>(value);
        } catch (const std::bad_variant_access &e) {
            GST_ERROR("Bad variant access in any_to_gvalue: %s", e.what());
        } catch (const std::bad_cast &e) {
            GST_ERROR("Bad cast in any_to_gvalue: %s", e.what());
        } catch (const std::exception &e) {
            GST_ERROR("Unknown exception in any_to_gvalue: %s", e.what());
        }
        gst_value_set_fraction(gvalue, fraction.first, fraction.second);
    }
}

static inline GParamSpec *param_desc_to_spec(const ParamDesc &param, GstStructure *enums_storage) {
    Any default_value = param.default_value;
    auto &range = param.range;
    constexpr auto param_flags = static_cast<GParamFlags>(G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY);
    if (any_holds_type<int>(default_value)) {
        gint min = (range.size() > 0) ? any_cast<int>(param.range[0]) : INT_MIN;
        gint max = (range.size() > 1) ? any_cast<int>(param.range[1]) : INT_MAX;
        return g_param_spec_int(param.name.c_str(), param.name.c_str(), param.description.c_str(), min, max,
                                any_cast<int>(default_value), param_flags);
    }
    if (any_holds_type<double>(default_value)) {
        double min = (range.size() > 0) ? any_cast<double>(param.range[0]) : -std::numeric_limits<double>::max();
        double max = (range.size() > 1) ? any_cast<double>(param.range[1]) : std::numeric_limits<double>::max();
        return g_param_spec_double(param.name.c_str(), param.name.c_str(), param.description.c_str(), min, max,
                                   any_cast<double>(default_value), param_flags);
    }
    if (any_holds_type<bool>(default_value))
        return g_param_spec_boolean(param.name.c_str(), param.name.c_str(), param.description.c_str(),
                                    any_cast<bool>(default_value), param_flags);
    if (any_holds_type<std::string>(default_value)) {
        if (param.range.empty() || !enums_storage) {
            return g_param_spec_string(param.name.c_str(), param.name.c_str(), param.description.c_str(),
                                       any_cast<std::string>(default_value).c_str(), param_flags);
        } else {
            int num_values = (int)param.range.size();
            int default_enum_val = 0;
            std::vector<GEnumValue> enum_values(num_values + 1);
            for (int i = 0; i < num_values; i++) {
                const std::string &str = std::get<std::string>(param.range[i]);
                enum_values[i] = {i, str.data(), str.data()};
                if (str == any_cast<std::string>(default_value))
                    default_enum_val = i;
            }
            enum_values[num_values] = {0, NULL, NULL};
            GVariant *gvariant_array = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, enum_values.data(),
                                                                 enum_values.size() * sizeof(GEnumValue), 1);
            gst_structure_set(enums_storage, param.name.data(), G_TYPE_VARIANT, gvariant_array, NULL);
            gsize size = 0;
            auto _enum_values = g_variant_get_fixed_array(
                g_value_get_variant(gst_structure_get_value(enums_storage, param.name.data())), &size, 1);
            GType gtype = g_enum_register_static(param.name.c_str(), (GEnumValue *)_enum_values);
            return g_param_spec_enum(param.name.c_str(), param.name.c_str(), param.description.c_str(), gtype,
                                     default_enum_val, param_flags);
        }
    }
    if (any_holds_type<intptr_t>(default_value))
        return g_param_spec_pointer(param.name.c_str(), param.name.c_str(), param.description.c_str(), param_flags);
    if (any_holds_type<std::vector<int>>(default_value)) {
        auto int_spec = g_param_spec_int(param.name.c_str(), param.name.c_str(), param.description.c_str(), INT_MIN,
                                         INT_MAX, 0, param_flags);
        return gst_param_spec_array(param.name.c_str(), param.name.c_str(), param.description.c_str(), int_spec,
                                    param_flags);
    }
    if (any_holds_type<std::vector<double>>(default_value)) {
        auto double_max = std::numeric_limits<double>::max();
        auto double_spec = g_param_spec_double(param.name.c_str(), param.name.c_str(), param.description.c_str(),
                                               -double_max, double_max, 0, param_flags);
        return gst_param_spec_array(param.name.c_str(), param.name.c_str(), param.description.c_str(), double_spec,
                                    param_flags);
    }
    if (any_holds_type<std::vector<std::string>>(default_value)) {
        auto string_spec =
            g_param_spec_string(param.name.c_str(), param.name.c_str(), param.description.c_str(), "", param_flags);
        return gst_param_spec_array(param.name.c_str(), param.name.c_str(), param.description.c_str(), string_spec,
                                    param_flags);
    }
    if (any_holds_type<std::pair<int, int>>(default_value)) {
        auto min = (range.size() > 0) ? any_cast<std::pair<int, int>>(param.range[0]) : std::pair<int, int>(INT_MIN, 1);
        auto max = (range.size() > 1) ? any_cast<std::pair<int, int>>(param.range[1]) : std::pair<int, int>(INT_MAX, 1);
        auto dfl = any_cast<std::pair<int, int>>(default_value);
        return gst_param_spec_fraction(param.name.c_str(), param.name.c_str(), param.description.c_str(), min.first,
                                       min.second, max.first, max.second, dfl.first, dfl.second, param_flags);
    }
    throw std::runtime_error("Unsupported parameter type");
}

static inline std::string get_property_as_string(GObject *object, const gchar *name) {
    GParamSpec *pspec = g_object_class_find_property(G_OBJECT_GET_CLASS(object), name);
    if (!pspec)
        return "";
    GValue v = {};
    g_value_init(&v, pspec->value_type);
    g_object_get_property(object, pspec->name, &v);
    gchar *contents = g_strdup_value_contents(&v);
    std::string str = contents;
    g_free(contents);
    g_value_unset(&v);
    if (str.front() == '"' && str.back() == '"')
        return str.substr(1, str.length() - 2);
    else
        return str;
}

} // namespace dlstreamer
