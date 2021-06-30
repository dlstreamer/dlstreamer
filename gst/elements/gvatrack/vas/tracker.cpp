/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tracker.h"
#include "mapped_mat.h"

#include "gva_buffer_map.h"
#include "gva_utils.h"
#include "tracker_gpu_loader.h"
#include "utils.h"
#include "video_frame.h"

#include "config.h"

#include <functional>

using namespace VasWrapper;

namespace {

constexpr int DEFAULT_MAX_NUM_OBJECTS = -1;
constexpr bool DEFAULT_TRACKING_PER_CLASS = true;
constexpr int NO_ASSOCIATION = -1;

inline bool CaseInsCharCompareN(char a, char b) {
    return (toupper(a) == toupper(b));
}

inline bool CaseInsCompare(const std::string &s1, const std::string &s2) {
    return ((s1.size() == s2.size()) && std::equal(s1.begin(), s1.end(), s2.begin(), CaseInsCharCompareN));
}

vas::BackendType backendType(const std::string &backend_type) {
    if (CaseInsCompare(backend_type, "CPU")) {
        return vas::BackendType::CPU;
    } else if (CaseInsCompare(backend_type, "VPU")) {
        return vas::BackendType::VPU;
    } else if (CaseInsCompare(backend_type, "GPU")) {
        return vas::BackendType::GPU;
    } else if (CaseInsCompare(backend_type, "FPGA")) {
        return vas::BackendType::FPGA;
    } else if (CaseInsCompare(backend_type, "HDDL")) {
        return vas::BackendType::HDDL;
    } else {
        throw std::invalid_argument("Unknown tracking device " + backend_type);
    }
}

vas::ColorFormat ConvertFormat(GstVideoFormat format) {
    switch (format) {
    case GST_VIDEO_FORMAT_BGR:
        return vas::ColorFormat::BGR;
    case GST_VIDEO_FORMAT_BGRx:
        return vas::ColorFormat::BGRX;
    case GST_VIDEO_FORMAT_BGRA:
        return vas::ColorFormat::BGRX;
    case GST_VIDEO_FORMAT_NV12:
        return vas::ColorFormat::NV12;
    case GST_VIDEO_FORMAT_I420:
        return vas::ColorFormat::I420;
    default:
        return vas::ColorFormat::BGR;
    }
}

std::vector<vas::ot::DetectedObject> extractDetectedObjects(GVA::VideoFrame &video_frame,
                                                            std::unordered_map<int, std::string> &labels) {
    std::vector<vas::ot::DetectedObject> detected_objects;
    for (GVA::RegionOfInterest &roi : video_frame.regions()) {
        int label_id = roi.detection().get_int("label_id", std::numeric_limits<int>::max());
        if (labels.find(label_id) == labels.end())
            labels[label_id] = roi.label();
        auto rect = roi.rect();
        cv::Rect obj_rect(rect.x, rect.y, rect.w, rect.h);
        detected_objects.emplace_back(obj_rect, label_id);
    }
    return detected_objects;
}

void append(GVA::VideoFrame &video_frame, const vas::ot::Object &tracked_object, const std::string &label) {
    auto roi = video_frame.add_region(tracked_object.rect.x, tracked_object.rect.y, tracked_object.rect.width,
                                      tracked_object.rect.height, label, 1.0);
    roi.detection().set_int("label_id", tracked_object.class_label);
    roi.set_object_id(tracked_object.tracking_id);
}

} // namespace

Tracker::Tracker(const GstGvaTrack *gva_track, vas::ot::TrackingType tracking_type)
    : gva_track(gva_track), tracker_type(tracking_type) {
    if (!gva_track) {
        throw std::invalid_argument("Tracker::Tracker: nullptr arguments is not allowed");
    }

    if (gva_track->info->finfo->format == GST_VIDEO_FORMAT_NV12 ||
        gva_track->info->finfo->format == GST_VIDEO_FORMAT_I420) {
        cv_empty_mat = cv::Mat(cv::Size(gva_track->info->width, gva_track->info->height * 1.5f), CV_8UC3);
    } else {
        cv_empty_mat = cv::Mat(cv::Size(gva_track->info->width, gva_track->info->height), CV_8UC3);
    }

    // Parse device string. Examples: VPU.1, CPU, VPU, etc.
    std::vector<std::string> full_device = Utils::splitString(gva_track->device, '.');
    auto backend = backendType(full_device[0]);

    const bool use_gpu_so =
        backend == vas::BackendType::GPU && (tracking_type == vas::ot::TrackingType::ZERO_TERM ||
                                             tracking_type == vas::ot::TrackingType::ZERO_TERM_COLOR_HISTOGRAM);
    if (use_gpu_so) {
        if (!VasOtGPULibBinder::get().is_loaded())
            throw std::runtime_error("Failed to load vasot GPU library. " + Utils::dpcppInstructionMsg);

        builder = VasOtGPULibBinder::get().createBuilder();
    } else {
        builder = std::unique_ptr<vas::ot::ObjectTracker::Builder>(new vas::ot::ObjectTracker::Builder());
    }

    builder->input_image_format = ConvertFormat(gva_track->info->finfo->format);

    // Initialize with defaults
    builder->max_num_objects = DEFAULT_MAX_NUM_OBJECTS;
    builder->tracking_per_class = DEFAULT_TRACKING_PER_CLASS;

    // Parse tracking configuration
    auto cfg = Utils::stringToMap(gva_track->tracking_config ? gva_track->tracking_config : std::string());
    auto iter = cfg.end();
    try {
        iter = cfg.find("max_num_objects");
        if (iter != cfg.end()) {
            builder->max_num_objects = std::stoi(iter->second);
            cfg.erase(iter);
        }

        iter = cfg.find("tracking_per_class");
        if (iter != cfg.end()) {
            builder->tracking_per_class = Utils::strToBool(iter->second);
            cfg.erase(iter);
        }
    } catch (...) {
        if (iter == cfg.end())
            std::throw_with_nested(std::runtime_error("Error occured while parsing key/value parameters"));
        std::throw_with_nested(std::runtime_error("Invalid value provided for parameter: " + iter->first));
    }

    GST_INFO_OBJECT(gva_track, "Tracker configuration:");
    GST_INFO_OBJECT(gva_track, "-- tracking_type: %d", static_cast<int>(tracker_type));
    GST_INFO_OBJECT(gva_track, "-- input_image_format: %d", static_cast<int>(builder->input_image_format));
    GST_INFO_OBJECT(gva_track, "-- max_num_objects: %#x", builder->max_num_objects);
    GST_INFO_OBJECT(gva_track, "-- tracking_per_class: %u", builder->tracking_per_class);

    builder->backend_type = backend;
    GST_INFO_OBJECT(gva_track, "-- backend_type: %d", static_cast<int>(builder->backend_type));

    if ((builder->backend_type == vas::BackendType::VPU || builder->backend_type == vas::BackendType::GPU) &&
        full_device.size() > 1) {
        cfg["device_id"] = full_device[1];
        GST_INFO_OBJECT(gva_track, "-- device_id: %s", cfg["device_id"].c_str());
    }

    builder->platform_config = std::move(cfg);
    if (use_gpu_so) {
        // TODO: Need to get va display
        // object_tracker = builder.Build(va_context.DisplayRaw(), tracker_type);
    } else {
        object_tracker = builder->Build(tracker_type);
    }
}

void Tracker::track(GstBuffer *buffer) {
    if (buffer == nullptr)
        throw std::invalid_argument("buffer is nullptr");
    try {
        GVA::VideoFrame video_frame(buffer, gva_track->info);
        std::vector<vas::ot::DetectedObject> detected_objects = extractDetectedObjects(video_frame, labels);

        std::vector<GVA::RegionOfInterest> regions = video_frame.regions();
        std::vector<vas::ot::Object> tracked_objects;

        // For imageless algorithms image is not important, so in that case static cv::Mat is passed thus avoiding
        // redundant buffer map/unmap operations
        if (tracker_type == vas::ot::TrackingType::ZERO_TERM_IMAGELESS ||
            tracker_type == vas::ot::TrackingType::SHORT_TERM_IMAGELESS) {
            tracked_objects = object_tracker->Track(cv_empty_mat, detected_objects);
#ifdef ENABLE_VAAPI
        } else if (builder->backend_type == vas::BackendType::GPU) {
            tracked_objects = trackGPU(buffer, detected_objects);
#endif
        } else {
            MappedMat cv_mat(buffer, gva_track->info, GST_MAP_READ);
            tracked_objects = object_tracker->Track(cv_mat.mat(), detected_objects);
        }

        for (const auto &tracked_object : tracked_objects) {
            if (tracked_object.status == vas::ot::TrackingStatus::LOST)
                continue;
            if (tracked_object.association_idx != NO_ASSOCIATION)
                regions[tracked_object.association_idx].set_object_id(tracked_object.tracking_id);
            else {
                auto it = labels.find(tracked_object.class_label);
                std::string label = it != labels.end() ? it->second : std::string();
                append(video_frame, tracked_object, label);
            }
        }
    } catch (const std::exception &e) {
        GST_ERROR("Exception within tracker occured: %s", e.what());
        throw std::runtime_error("Track: error while tracking objects");
    }
}

#ifdef ENABLE_VAAPI
std::vector<vas::ot::Object> Tracker::trackGPU(GstBuffer *buffer,
                                               const std::vector<vas::ot::DetectedObject> &detected_objects) {
    using namespace InferenceBackend;

    MemoryType mem_type;
    switch (gva_track->caps_feature) {
    case DMA_BUF_CAPS_FEATURE:
        mem_type = MemoryType::DMA_BUFFER;
        break;
    case VA_SURFACE_CAPS_FEATURE:
        mem_type = MemoryType::VAAPI;
        break;
    default:
        throw std::runtime_error("Unsupported buffer memory type");
    }

    Image image;
    BufferMapContext mapContext;
    gva_buffer_map(buffer, image, mapContext, gva_track->info, mem_type, GST_MAP_READ);
    auto mapContextPtr = std::unique_ptr<BufferMapContext, std::function<void(BufferMapContext *)>>(
        &mapContext, [](BufferMapContext *mapContext) { gva_buffer_unmap(*mapContext); });

    VADisplay display = image.va_display;
    VASurfaceID surface_id = image.va_surface_id;

    if (mem_type == MemoryType::DMA_BUFFER) {
        if (!vaapi_converter || !vaapi_context) {
            vaapi_context = std::unique_ptr<VaApiContext>(new VaApiContext());
            vaapi_converter = std::unique_ptr<VaApiConverter>(new VaApiConverter(vaapi_context.get()));
        }

        auto dst_va_image = std::unique_ptr<VaApiImage>(
            new VaApiImage(vaapi_context.get(), image.width, image.height, FourCC::FOURCC_NV12, MemoryType::VAAPI));

        vaapi_converter->Convert(image, *dst_va_image);

        auto image_map = dst_va_image->Map();
        display = image_map.va_display;
        surface_id = image_map.va_surface_id;
        dst_va_image->Unmap();
    }

    // TODO: Need to get original va display from context created by vaapi elements
    if (!object_tracker) {
        object_tracker = VasOtGPULibBinder::get().createGPUTracker(builder.get(), display, tracker_type);
    }

    return VasOtGPULibBinder::get().runTrackGPU(object_tracker.get(), surface_id, image.width, image.height,
                                                detected_objects);
}

#endif