/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/opencv/elements/opencv_object_association.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/cpu/context.h"
#include "dlstreamer/cpu/utils.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/opencv/context.h"
#include "dlstreamer/opencv/mappers/cpu_to_opencv.h"
#include "dlstreamer/opencv/tensor.h"
#include "ot.h"

namespace dlstreamer {

namespace param {
static constexpr auto shape_feature_weight = "shape-feature-weight";
static constexpr auto trajectory_feature_weight = "trajectory-feature-weight";
static constexpr auto spatial_feature_weight = "spatial-feature-weight";
static constexpr auto spatial_feature_metadata_name = "spatial-feature-metadata-name";
static constexpr auto spatial_feature_distance = "spatial-feature-distance";
} // namespace param

static ParamDescVector params_desc = {
    {param::shape_feature_weight, "Weighting factor for shape-based feature", 0.75, 0.0, 1.0},
    {param::trajectory_feature_weight, "Weighting factor for trajectory-based feature", 0.5, 0.0, 1.0},
    {param::spatial_feature_weight, "Weighting factor for spatial feature", 0.25, 0.0, 1.0},
    {param::spatial_feature_metadata_name, "Name of metadata containing spatial feature", "spatial-feature"},
    {param::spatial_feature_distance,
     "Method to calculate distance between two spatial features",
     "none",
     {"none", "cosine", "bhattacharyya"}}};

class ObjectAssociationOpenCV : public BaseTransformInplace {
  public:
    ObjectAssociationOpenCV(DictionaryCPtr params, const ContextPtr &app_context) : BaseTransformInplace(app_context) {
        _metadata_name = params->get<std::string>(param::spatial_feature_metadata_name);
        _spatial_feature_distance = params->get<std::string>(param::spatial_feature_distance);

        vas::ot::ObjectTracker::Builder ot_builder;
        ot_builder.kRgbHistDistScale = params->get<double>(param::spatial_feature_weight);
        ot_builder.kNormCenterDistScale = params->get<double>(param::trajectory_feature_weight);
        ot_builder.kNormShapeDistScale = params->get<double>(param::shape_feature_weight);
        vas::ot::TrackingType tracking_type = (_spatial_feature_distance == "none")
                                                  ? vas::ot::TrackingType::ZERO_TERM_IMAGELESS
                                                  : vas::ot::TrackingType::ZERO_TERM;
        _tracker = ot_builder.Build(tracking_type);
    }

    bool init_once() override {
        auto cpu_context = std::make_shared<CPUContext>();
        auto opencv_context = std::make_shared<OpenCVContext>();
        _opencv_mapper = create_mapper({cpu_context, opencv_context});

        return true;
    }

    bool process(FramePtr src) override {
        ImageInfo src_info(src->tensor(0)->info());
        cv::Size frame_size(src_info.width(), src_info.height());
        auto regions = src->regions();

        // Get features (Histogram data or ReId inference result) from metadata
        TensorVector features(regions.size());
        if (_spatial_feature_distance == "none") {
            for (size_t i = 0; i < regions.size(); i++) {
                auto meta = find_metadata<InferenceResultMetadata>(*regions[i], _metadata_name);
                if (meta)
                    features[i] = _opencv_mapper->map(meta->tensor(), AccessMode::Read);
                else
                    throw std::runtime_error("Can't find metadata with name=" + _metadata_name);
            }
        }

        // DetectedObject = rectangle + feature
        std::vector<vas::ot::DetectedObject> objects;
        for (size_t i = 0; i < regions.size(); i++) {
            auto &region = regions[i];
            auto detection_meta = find_metadata<DetectionMetadata>(*region);
            DLS_CHECK(detection_meta)
            auto region_tensor = region->tensor(0);
            ImageInfo region_info(region_tensor->info());
            auto offset_x = std::lround(detection_meta->x_min() * src_info.width());
            auto offset_y = std::lround(detection_meta->y_min() * src_info.height());
            cv::Rect rect(offset_x, offset_y, region_info.width(), region_info.height());
            cv::Mat feature = features[i] ? ptr_cast<OpenCVTensor>(features[i])->cv_mat() : cv::Mat();
            objects.push_back({rect, 0, feature});
        }

        // Run tracker
        auto tracked_objects = _tracker->Track(frame_size, objects);
        for (const auto &tracked_object : tracked_objects) {
            if (tracked_object.status == vas::ot::TrackingStatus::LOST)
                continue;
            if (tracked_object.association_idx >= 0) {
                auto objectid_meta = add_metadata<ObjectIdMetadata>(*regions[tracked_object.association_idx]);
                objectid_meta.set_id(tracked_object.tracking_id);
            } /*else { TODO
                auto it = labels.find(tracked_object.class_label);
                std::string label = it != labels.end() ? it->second : std::string();
                append(frame_meta, tracked_object, label);
            }*/
        }

        return true;
    }

  protected:
    std::string _metadata_name;
    std::string _spatial_feature_distance;
    MemoryMapperPtr _opencv_mapper;
    std::unique_ptr<vas::ot::ObjectTracker> _tracker;
};

extern "C" {
ElementDesc opencv_object_association = {.name = "opencv_object_association",
                                         .description = "Assigns unique ID to ROI objects based on objects trajectory "
                                                        "and (optionally) feature vector obtained from ROI metadata",
                                         .author = "Intel Corporation",
                                         .params = &params_desc,
                                         .input_info = {},
                                         .output_info = {},
                                         .create = create_element<ObjectAssociationOpenCV>,
                                         .flags = 0};
}

} // namespace dlstreamer
