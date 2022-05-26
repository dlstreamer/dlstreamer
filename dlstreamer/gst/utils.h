/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"

#include "dlstreamer/buffer.h"
#include "dlstreamer/utils.h"
#include <gst/video/gstvideometa.h>

namespace dlstreamer {

// TODO: duplicates MEDIA_NAME from `preview_elements/capabilities.h`
// Move it to common place after chaos with cmake dependencies is resolved
#define DLS_TENSOR_MEDIA_NAME "other/tensors"

static inline FourCC gst_format_to_fourcc(int format) {
    switch (format) {
    case GST_VIDEO_FORMAT_BGR:
        return FourCC::FOURCC_BGR;
    case GST_VIDEO_FORMAT_RGB:
        return FourCC::FOURCC_RGB;
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_BGRx:
        return FourCC::FOURCC_BGRX;
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_RGBx:
        return FourCC::FOURCC_RGBX;
    case GST_VIDEO_FORMAT_GBR:
        return FourCC::FOURCC_RGBP;
#if (GST_VERSION_MAJOR >= 1) && (GST_VERSION_MINOR >= 20)
    case GST_VIDEO_FORMAT_BGRP:
        return FourCC::FOURCC_BGRP;
#endif
    case GST_VIDEO_FORMAT_NV12:
        return FourCC::FOURCC_NV12;
    case GST_VIDEO_FORMAT_I420:
        return FourCC::FOURCC_I420;
    default:
        throw std::runtime_error("Unsupported GST_VIDEO_FORMAT: " + std::to_string(format));
    }
}

static inline GstVideoFormat fourcc_to_gst_format(FourCC format) {
    switch (format) {
    case FourCC::FOURCC_BGR:
        return GST_VIDEO_FORMAT_BGR;
    case FourCC::FOURCC_RGB:
        return GST_VIDEO_FORMAT_RGB;
    case FourCC::FOURCC_BGRX:
        return GST_VIDEO_FORMAT_BGRA;
    case FourCC::FOURCC_RGBX:
        return GST_VIDEO_FORMAT_RGBA;
    case FourCC::FOURCC_RGBP:
        return GST_VIDEO_FORMAT_GBR;
    case FourCC::FOURCC_BGRP:
#if (GST_VERSION_MAJOR >= 1) && (GST_VERSION_MINOR >= 20)
        return GST_VIDEO_FORMAT_BGRP;
#else
        throw std::runtime_error("Unsupported FourCC: %d" + std::to_string(format));
#endif
    case FourCC::FOURCC_NV12:
        return GST_VIDEO_FORMAT_NV12;
    case FourCC::FOURCC_I420:
        return GST_VIDEO_FORMAT_I420;
    }
    throw std::runtime_error("Unsupported FourCC: %d" + std::to_string(format));
}

static inline std::shared_ptr<BufferInfo> gst_video_info_to_buffer_info(const GstVideoInfo *vinfo) {
    if (!vinfo)
        throw std::runtime_error("video info is NULL");

    auto info = std::make_shared<BufferInfo>();

    FourCC fourcc = gst_format_to_fourcc(GST_VIDEO_INFO_FORMAT(vinfo));
    info->media_type = MediaType::VIDEO;
    info->format = static_cast<int>(fourcc);

    const guint n_planes = GST_VIDEO_INFO_N_PLANES(vinfo);
    for (guint i = 0; i < n_planes; i++) {
        size_t width = GST_VIDEO_INFO_COMP_WIDTH(vinfo, i);
        size_t height = GST_VIDEO_INFO_COMP_HEIGHT(vinfo, i);
        size_t stride = GST_VIDEO_INFO_PLANE_STRIDE(vinfo, i);
        size_t offset = GST_VIDEO_INFO_PLANE_OFFSET(vinfo, i);
        size_t channels = vinfo->finfo->pixel_stride[i];

        PlaneInfo plane({height, width, channels}, DataType::U8, "", {stride, channels, 1});
        plane.offset = offset;
        info->planes.push_back(plane);
    }

    // planar formats with several identical planes (ex, RGBP)
    if (n_planes > 1) {
        bool identical = true;
        const PlaneInfo &plane0 = info->planes[0];
        for (guint i = 1; i < n_planes; i++) {
            const PlaneInfo &planei = info->planes[i];
            if (planei.shape != plane0.shape || planei.stride != plane0.stride)
                identical = false;
        }
        if (identical && info->planes[0].channels() == 1) {
            size_t width = GST_VIDEO_INFO_COMP_WIDTH(vinfo, 0);
            size_t height = GST_VIDEO_INFO_COMP_HEIGHT(vinfo, 0);
            size_t stride = GST_VIDEO_INFO_PLANE_STRIDE(vinfo, 0);
            size_t offset = GST_VIDEO_INFO_PLANE_OFFSET(vinfo, 1); // offset of second plane

            PlaneInfo plane({n_planes, height, width}, DataType::U8, "", {offset, stride, 1});
            info->planes = {plane};
        }
    }

    return info;
}

static inline GstCapsFeatures *buffer_type_to_gst_caps_feature(BufferType buffer_type) {
    GstCapsFeatures *features = nullptr;
    switch (buffer_type) {
    case BufferType::UNKNOWN:
    case BufferType::GST_BUFFER:
        features = gst_caps_features_new_any();
        break;
    case BufferType::CPU:
        features = gst_caps_features_new_empty();
        break;
    default: {
        std::string str = std::string("memory:").append(buffer_type_to_string(buffer_type));
        features = gst_caps_features_from_string(str.c_str());
    } break;
    }
    return features;
}

static inline GstCaps *buffer_info_to_gst_caps(const BufferInfo &info) {
    GstStructure *structure = nullptr;
    if (info.media_type == MediaType::ANY) {
        return gst_caps_new_any();
    } else if (info.media_type == MediaType::VIDEO) {
        structure = gst_structure_new_empty("video/x-raw");
        if (info.format) {
            GstVideoFormat gst_format = fourcc_to_gst_format(static_cast<FourCC>(info.format));
            gst_structure_set(structure, "format", G_TYPE_STRING, gst_video_format_to_string(gst_format), NULL);
        }
        if (!info.planes.empty()) {
            gst_structure_set(structure, "width", G_TYPE_INT, info.planes[0].width(), NULL);
            gst_structure_set(structure, "height", G_TYPE_INT, info.planes[0].height(), NULL);
        }
    } else if (info.media_type == MediaType::TENSORS) {
        structure = gst_structure_new_empty(DLS_TENSOR_MEDIA_NAME);
        size_t num_tensors = info.planes.size();
        if (num_tensors) {
            std::string precision_str;
            std::string dims_str;
            for (size_t i = 0; i < num_tensors; i++) {
                if (i) {
                    precision_str += ",";
                    dims_str += ",";
                }
                precision_str += datatype_to_string(info.planes[i].type);
                dims_str += shape_to_string(info.planes[i].shape);
            }
            gst_structure_set(structure, "num_tensors", G_TYPE_INT, num_tensors, NULL);
            // gst_structure_set(structure, "format", G_TYPE_STRING, "static", NULL);
            if (!precision_str.empty())
                gst_structure_set(structure, "types", G_TYPE_STRING, precision_str.c_str(), NULL);
            if (!dims_str.empty())
                gst_structure_set(structure, "dimensions", G_TYPE_STRING, dims_str.c_str(), NULL);
        }
    } else {
        throw std::runtime_error("Unsupported MediaType");
    }

    GstCaps *caps = gst_caps_new_full(structure, nullptr);
    gst_caps_set_features(caps, 0, buffer_type_to_gst_caps_feature(info.buffer_type));
    return caps;
}

static inline GstCaps *buffer_info_vector_to_gst_caps(const BufferInfoVector &infos) {
    GstCaps *result = gst_caps_new_empty();
    for (auto &&info : infos) {
        auto caps = buffer_info_to_gst_caps(info);
        gst_caps_append(result, caps);
    }
    return result;
}

static inline BufferInfo gst_caps_to_buffer_info(const GstCaps *caps, guint index = 0) {
    BufferInfo info;
    GstStructure *caps_str = gst_caps_get_structure(caps, index);
    std::string media_type = gst_structure_get_name(caps_str);
    std::string feature = gst_caps_features_to_string(gst_caps_get_features(caps, index));
    if (feature.rfind("memory:", 0) == 0) {
        info.buffer_type = buffer_type_from_string(feature.substr(7));
    }
    if (media_type == "video/x-raw") {
        GstVideoInfo video_info = {};
        if (gst_caps_is_fixed(caps) && gst_video_info_from_caps(&video_info, caps)) {
            info = *gst_video_info_to_buffer_info(&video_info);
        }
        return info;
    } else if (media_type == DLS_TENSOR_MEDIA_NAME) {
        info.media_type = MediaType::TENSORS;
        int num_tensors = 0;
        gst_structure_get_int(caps_str, "num_tensors", &num_tensors);
        if (num_tensors) {
            auto types_str = gst_structure_get_string(caps_str, "types");
            auto shapes_str = gst_structure_get_string(caps_str, "dimensions");
            auto name_str = gst_structure_get_string(caps_str, "name");
            if (!types_str)
                throw std::invalid_argument("Tensor type not specified in caps structure");
            if (!shapes_str)
                shapes_str = "";
            auto types_array = split_string(types_str, ',');
            auto shapes_array = split_string(shapes_str, ',');

            for (int i = 0; i < num_tensors; i++) {
                DataType type = datatype_from_string(types_array[i]);
                auto shape = shapes_array.size() ? shape_from_string(shapes_array[i]) : std::vector<size_t>();
                info.planes.push_back(PlaneInfo(shape, type, name_str ? name_str : ""));
            }
        }
    } else {
        throw std::runtime_error("Unsupported media type " + media_type);
    }
    return info;
}

static inline GstContext *gst_query_context(GstPad *pad, const gchar *context_name) {
    GstQuery *query = gst_query_new_context(context_name);
    auto query_unref = std::shared_ptr<GstQuery>(query, [](GstQuery *query) { gst_query_unref(query); });

    gboolean ret = gst_pad_peer_query(pad, query);
    if (!ret)
        throw std::runtime_error("Couldn't query GST context: " + std::string(context_name));

    GstContext *context = nullptr;
    gst_query_parse_context(query, &context);
    if (!context)
        throw std::runtime_error("Error gst_query_parse_context");

    gst_context_ref(context);
    GST_INFO_OBJECT(pad, "Got GST context: %" GST_PTR_FORMAT, context);
    return context;
}

} // namespace dlstreamer
