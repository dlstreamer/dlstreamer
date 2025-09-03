/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/opencv/elements/opencv_object_association.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/cpu/context.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/opencv/context.h"
#include "object_tracker.h"

namespace dlstreamer {

namespace param {
static constexpr auto generate_objects = "generate-objects";
static constexpr auto adjust_objects = "adjust-objects";
static constexpr auto tracking_per_class = "tracking-per-class";
static constexpr auto spatial_feature_distance = "spatial-feature-distance";
static constexpr auto spatial_feature_metadata_name = "spatial-feature-metadata-name";

static constexpr auto shape_feature_weight = "shape-feature-weight";
static constexpr auto trajectory_feature_weight = "trajectory-feature-weight";
static constexpr auto spatial_feature_weight = "spatial-feature-weight";
static constexpr auto min_region_ratio_in_boundary = "min-region-ratio-in-boundary";
} // namespace param

static ParamDescVector params_desc = {
    {param::generate_objects,
     "If true, generate objects (according to previous trajectory) if not detected on current frame", true},
    {param::adjust_objects, "If true, adjust object position for more smooth trajectory", true},
    {param::tracking_per_class, "If true, object association takes into account object class", false},
    {param::spatial_feature_metadata_name, "Name of metadata containing spatial feature", "spatial-feature"},
    {param::spatial_feature_distance,
     "Method to calculate distance between two spatial features",
     "bhattacharyya",
     {"none", "cosine", "bhattacharyya"}},
    // object tracker tuning parameters
    {param::shape_feature_weight, "Weighting factor for shape-based feature", 0.75, 0.0, 1.0},
    {param::trajectory_feature_weight, "Weighting factor for trajectory-based feature", 0.5, 0.0, 1.0},
    {param::spatial_feature_weight, "Weighting factor for spatial feature", 0.25, 0.0, 1.0},
    {param::min_region_ratio_in_boundary, " Min region ratio in image boundary", 0.75, 0.0, 1.0},
};

class ObjectAssociationOpenCV : public BaseTransformInplace {
  public:
    ObjectAssociationOpenCV(DictionaryCPtr params, const ContextPtr &app_context) : BaseTransformInplace(app_context) {
        _adjust_objects = params->get<bool>(param::adjust_objects, true);
        _metadata_name = params->get<std::string>(param::spatial_feature_metadata_name);
        _spatial_feature_distance = params->get<std::string>(param::spatial_feature_distance);

        _ot_params.generate_objects = params->get<bool>(param::generate_objects, true);
        _ot_params.tracking_per_class = params->get<bool>(param::tracking_per_class, true);
        _ot_params.kRgbHistDistScale = params->get<double>(param::spatial_feature_weight);
        _ot_params.kNormCenterDistScale = params->get<double>(param::trajectory_feature_weight);
        _ot_params.kNormShapeDistScale = params->get<double>(param::shape_feature_weight);
        _ot_params.min_region_ratio_in_boundary = params->get<double>(param::min_region_ratio_in_boundary);
        _tracker = std::make_unique<vas::ot::ObjectTracker>(_ot_params);
    }

    bool init_once() override {
        auto cpu_context = std::make_shared<CPUContext>();
        auto opencv_context = std::make_shared<OpenCVContext>();
        _opencv_mapper = create_mapper({cpu_context, opencv_context});

        return true;
    }

    bool process(FramePtr frame) override {
        ImageInfo frame_info(frame->tensor(0)->info());
        cv::Size frame_size(frame_info.width(), frame_info.height());
        DLS_CHECK(frame_size.width && frame_size.height);
        double frame_width = frame_size.width;
        double frame_height = frame_size.height;

        // for each region, create DetectedObject = rectangle + label_id + feature
        auto regions = frame->regions();
        std::vector<vas::ot::DetectedObject> objects;
        TensorVector features;
        for (size_t i = 0; i < regions.size(); i++) {
            auto &region = regions[i];

            // rectangle
            auto detection_meta = find_metadata<DetectionMetadata>(*region);
            DLS_CHECK(detection_meta)
            auto x = std::lround(detection_meta->x_min() * frame_width);
            auto y = std::lround(detection_meta->y_min() * frame_height);
            auto w = std::lround(detection_meta->x_max() * frame_width) - x;
            auto h = std::lround(detection_meta->y_max() * frame_height) - y;
            cv::Rect rect(x, y, w, h);

            // label_id
            int label_id = detection_meta->label_id();
            if (label_id_to_string.find(label_id) == label_id_to_string.end())
                label_id_to_string[label_id] = detection_meta->label();

            // feature (Histogram data or ReId inference result)
            cv::Mat feature;
            auto meta = find_metadata<InferenceResultMetadata>(*regions[i], _metadata_name);
            if (meta) {
                auto feature_tensor = _opencv_mapper->map(meta->tensor(), AccessMode::Read);
                feature = ptr_cast<OpenCVTensor>(feature_tensor)->cv_mat();
                features.push_back(feature_tensor); // keep reference while using cv::Mat object
            }

            objects.push_back({rect, label_id, feature});
            // logger("Frame%02d:     received roi=%d,%d,%d,%d\n", frame_num, rect.x, rect.y, rect.width, rect.height);
        }

        // Run tracker
        auto tracked_objects = _tracker->Track(frame_size, objects);

        // Create new ROI objects
        if (_ot_params.generate_objects) {
            int association_idx = objects.size(); // index for new objects
            for (auto &tracked_object : tracked_objects) {
                if (tracked_object.status == vas::ot::TrackingStatus::LOST)
                    continue;
                if (tracked_object.association_idx < 0) { // new object created by tracker
                    DetectionMetadata dmeta(frame->metadata().add(DetectionMetadata::name));
                    int label_id = tracked_object.class_label;
                    dmeta.set(DetectionMetadata::key::label_id, label_id);
                    auto it = label_id_to_string.find(label_id);
                    if (it != label_id_to_string.end())
                        dmeta.set(DetectionMetadata::key::label, it->second);
                    // logger("Frame%02d: create object with index = %d\n", frame_num, association_idx);
                    tracked_object.association_idx = association_idx++;
                }
            }
            regions = frame->regions(); // update regions list
        }

        // Update object ID and rectangle
        for (const auto &tracked_object : tracked_objects) {
            if (tracked_object.status == vas::ot::TrackingStatus::LOST)
                continue;
            if (tracked_object.association_idx < 0)
                continue;
            size_t object_index = tracked_object.association_idx;
            DLS_CHECK(object_index < regions.size());
            auto &region = regions[object_index];
            auto objectid_meta = add_metadata<ObjectIdMetadata>(*region);
            objectid_meta.set_id(tracked_object.tracking_id);
            // logger("Frame%02d:    set id=%lu, roi=%d,%d,%d,%d\n", frame_num, tracked_object.tracking_id,
            //  tracked_object.rect.x, tracked_object.rect.y, tracked_object.rect.width, tracked_object.rect.height);
            if (_adjust_objects || object_index >= objects.size()) { // adjust input objects or set rect to new objects
                auto detection_meta = find_metadata<DetectionMetadata>(*region);
                DLS_CHECK(detection_meta)
                double x_min = tracked_object.rect.x / frame_width;
                double y_min = tracked_object.rect.y / frame_height;
                double x_max = (tracked_object.rect.x + tracked_object.rect.width) / frame_width;
                double y_max = (tracked_object.rect.y + tracked_object.rect.height) / frame_height;
                detection_meta->init(x_min, y_min, x_max, y_max);
            }
        }

        return true;
    }

  protected:
    bool _adjust_objects;
    std::string _metadata_name;
    std::string _spatial_feature_distance;
    MemoryMapperPtr _opencv_mapper;
    vas::ot::Tracker::InitParameters _ot_params;
    std::unique_ptr<vas::ot::ObjectTracker> _tracker;
    std::map<int, std::string> label_id_to_string;
};

extern "C" {
ElementDesc opencv_object_association = {.name = "opencv_object_association",
                                         .description = "Assigns unique ID to ROI objects based on objects trajectory "
                                                        "and (optionally) feature vector obtained from ROI metadata",
                                         .author = "Intel Corporation",
                                         .params = &params_desc,
                                         .input_info = MAKE_FRAME_INFO_VECTOR({}),
                                         .output_info = MAKE_FRAME_INFO_VECTOR({}),
                                         .create = create_element<ObjectAssociationOpenCV>,
                                         .flags = 0};
}

} // namespace dlstreamer
