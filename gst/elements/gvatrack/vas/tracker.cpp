/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"

#include "buffer_map/buffer_mapper.h"
#include "mapped_mat.h"
#include "tracker.h"

#include "gst_vaapi_helper.h"
#include "gva_utils.h"
#include "scope_guard.h"
#include "tracker_gpu_loader.h"
#include "utils.h"
#include "video_frame.h"

#include <functional>

#ifdef ENABLE_VAAPI
#include "vaapi_converter.h"
#endif

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

// Simple structure to keep all VAAPI related things in one place
struct Tracker::VaApiEntity {
#ifdef ENABLE_VAAPI

    VaApiEntity(VaApiDisplayPtr dpy) : context(dpy), converter(&context) {
    }

    InferenceBackend::VaApiContext context;
    InferenceBackend::VaApiConverter converter;

#else // !ENABLE_VAAPI

    VaApiEntity(VaApiDisplayPtr) {
        throw std::runtime_error("Couldn't create required VAAPI instances: project was built without VAAPI support");
    }

#endif
};

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
    backend_type = backendType(full_device[0]);

    const bool use_gpu_so =
        backend_type == vas::BackendType::GPU && (tracking_type == vas::ot::TrackingType::ZERO_TERM ||
                                                  tracking_type == vas::ot::TrackingType::ZERO_TERM_COLOR_HISTOGRAM);

    std::unique_ptr<vas::ot::ObjectTracker::Builder> builder;
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

    builder->backend_type = backend_type;
    GST_INFO_OBJECT(gva_track, "-- backend_type: %d", static_cast<int>(builder->backend_type));

    if ((builder->backend_type == vas::BackendType::VPU || builder->backend_type == vas::BackendType::GPU) &&
        full_device.size() > 1) {
        cfg["device_id"] = full_device.at(1);
        GST_INFO_OBJECT(gva_track, "-- device_id: %s", cfg["device_id"].c_str());
    }

    builder->platform_config = std::move(cfg);
    if (use_gpu_so) {
        buildGPUTracker(builder.get());
    } else {
        object_tracker = builder->Build(tracker_type);
        buffer_mapper = BufferMapperFactory::createMapper(InferenceBackend::MemoryType::SYSTEM, gva_track->info);
    }
}

void Tracker::track(GstBuffer *buffer) {
    if (buffer == nullptr)
        throw std::invalid_argument("buffer is nullptr");
    try {
        GVA::VideoFrame video_frame(buffer, gva_track->info);
        std::vector<vas::ot::DetectedObject> detected_objects = extractDetectedObjects(video_frame, labels);

        std::vector<vas::ot::Object> tracked_objects;
        if (backend_type == vas::BackendType::CPU)
            tracked_objects = trackCPU(buffer, detected_objects);
        else
            tracked_objects = trackGPU(buffer, detected_objects);

        std::vector<GVA::RegionOfInterest> regions = video_frame.regions();
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
        GST_ERROR("Exception within tracker occurred: %s", Utils::createNestedErrorMsg(e).c_str());
        throw std::runtime_error("Track: error while tracking objects");
    }
}

void Tracker::buildGPUTracker(vas::ot::ObjectTracker::Builder *builder) {
    assert(builder && "Builder cannot be null");

    InferenceBackend::MemoryType mem_type;
    switch (gva_track->caps_feature) {
    case DMA_BUF_CAPS_FEATURE:
        mem_type = InferenceBackend::MemoryType::DMA_BUFFER;
        break;
    case VA_SURFACE_CAPS_FEATURE:
        mem_type = InferenceBackend::MemoryType::VAAPI;
        break;
    default:
        throw std::runtime_error("Unsupported buffer memory type");
    }

    auto va_display = VaapiHelper::queryVaDisplay(const_cast<GstBaseTransform *>(&gva_track->base_transform));
    if (!va_display)
        throw std::runtime_error("Couldn't query VADisplay from VA-API elements. Possible reason: gstreamer-vaapi "
                                 "isn't built with required patches");

    vaapi.reset(new VaApiEntity(va_display));

    buffer_mapper = BufferMapperFactory::createMapper(mem_type, gva_track->info, va_display);

    object_tracker = VasOtGPULibBinder::get().createGPUTracker(builder, va_display.get(), tracker_type);
}

std::vector<vas::ot::Object> Tracker::trackCPU(GstBuffer *buffer,
                                               const std::vector<vas::ot::DetectedObject> &detected_objects) {

    if (!buffer)
        throw std::invalid_argument("trackCPU: buffer is null");
    assert(backend_type == vas::BackendType::CPU);
    assert(buffer_mapper->memoryType() == InferenceBackend::MemoryType::SYSTEM &&
           "Mapper to system memory is expected");

    // For imageless algorithms image is not important, so in that case static cv::Mat is passed thus avoiding
    // redundant buffer map/unmap operations
    if (tracker_type == vas::ot::TrackingType::ZERO_TERM_IMAGELESS ||
        tracker_type == vas::ot::TrackingType::SHORT_TERM_IMAGELESS) {
        return object_tracker->Track(cv_empty_mat, detected_objects);
    }

    MappedMat cv_mat(buffer, *buffer_mapper, GST_MAP_READ);
    return object_tracker->Track(cv_mat.mat(), detected_objects);
}

#ifdef ENABLE_VAAPI
std::vector<vas::ot::Object> Tracker::trackGPU(GstBuffer *buffer,
                                               const std::vector<vas::ot::DetectedObject> &detected_objects) {
    using namespace InferenceBackend;
    if (!buffer)
        throw std::invalid_argument("trackGPU: buffer is null");
    assert(backend_type == vas::BackendType::GPU);
    assert(vaapi);

    Image image = buffer_mapper->map(buffer, GST_MAP_READ);
    auto image_sg = makeScopeGuard([&] { buffer_mapper->unmap(image); });

    VASurfaceID surface_id = image.va_surface_id;

    if (image.type == MemoryType::DMA_BUFFER) {
        VaApiImage dst_va_image(&vaapi->context, image.width, image.height, FourCC::FOURCC_NV12, MemoryType::VAAPI);

        vaapi->converter.Convert(image, dst_va_image);

        auto image_map = dst_va_image.Map();
        surface_id = image_map.va_surface_id;
        dst_va_image.Unmap();
    }

    return VasOtGPULibBinder::get().runTrackGPU(object_tracker.get(), surface_id, image.width, image.height,
                                                detected_objects);
}
#else
std::vector<vas::ot::Object> Tracker::trackGPU(GstBuffer *, const std::vector<vas::ot::DetectedObject> &) {
    assert(false && "Couldn't run track on GPU: project was built without VAAPI support");
}
#endif
