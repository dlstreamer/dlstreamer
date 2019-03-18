/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "identify.h"

#include "config.h"
#include "gstgvaidentify.h"
#include "gva_roi_meta.h"
#include "gva_tensor_meta.h"
#include "gva_utils.h"

#include <opencv2/imgproc.hpp>
#include <vector>

#include "logger_functions.h"

#define UNUSED(x) (void)x

using namespace std::placeholders;

Identify::Identify(GstGvaIdentify *ovino) : frame_num(0) {
    masterGstElement_ = ovino;

    // Create gallery
    double reid_threshold = ovino->threshold;
    gallery = std::unique_ptr<EmbeddingsGallery>(new EmbeddingsGallery(ovino->gallery, reid_threshold));

    // Create tracker. TODO expose some of parameters as GST properties
    if (ovino->tracker) {
        TrackerParams tracker_reid_params;
        tracker_reid_params.min_track_duration = 1;
        tracker_reid_params.forget_delay = 150;
        tracker_reid_params.affinity_thr = 0.8;
        tracker_reid_params.averaging_window_size = 1;
        tracker_reid_params.bbox_heights_range = cv::Vec2f(10, 1080);
        tracker_reid_params.drop_forgotten_tracks = false;
        tracker_reid_params.max_num_objects_in_track = std::numeric_limits<int>::max();
        tracker_reid_params.objects_type = "face";
        tracker = std::unique_ptr<Tracker>(new Tracker(tracker_reid_params));
    } else {
        tracker = nullptr;
    }
}

Identify::~Identify() {
}

void Identify::ProcessOutput(GstBuffer *buffer, GstVideoInfo *info) {
    GVA::RegionOfInterestList roi_list(buffer);
    std::vector<GVA::Tensor *> tensors;
    std::vector<cv::Mat> embeddings;
    std::vector<TrackedObject> tracked_objects;

    // Compare embeddings versus gallery
    for (GVA::RegionOfInterest &roi : roi_list) {
        for (GVA::Tensor &tensor : roi) {
            auto s = tensor.model_name();
            if (masterGstElement_->model) {
                if (tensor.model_name().find(masterGstElement_->model) == std::string::npos)
                    continue;
            } else {
                if (tensor.format() != "cosine_distance")
                    continue;
            }
            // embeddings
            std::vector<float> data = tensor.data<float>();
            cv::Mat blob_wrapper(data.size(), 1, CV_32F, data.data());
            embeddings.emplace_back();
            blob_wrapper.copyTo(embeddings.back());
            // tracked_objects
            GstVideoRegionOfInterestMeta *meta = roi.meta();
            cv::Rect rect(meta->x, meta->y, meta->w, meta->h);
            tracked_objects.emplace_back(rect, roi.confidence(), 0, 0);
            // tensors
            tensors.push_back(&tensor);
            break;
        }
    }
    auto ids = gallery->GetIDsByEmbeddings(embeddings);

    // set object_index and label
    for (size_t i = 0; i < tracked_objects.size(); i++) {
        int label = ids.empty() ? EmbeddingsGallery::unknown_id : ids[i];
        tracked_objects[i].label = label;
        tracked_objects[i].object_index = i;
    }

    // Run tracker
    if (tracker) {
        frame_num++;
        tracker->Process(cv::Size(info->width, info->height), tracked_objects, frame_num);
        tracked_objects = tracker->TrackedDetections();
    }

    // Add object_id and label_id to metadata
    for (TrackedObject &obj : tracked_objects) {
        if (obj.object_index < 0 || (size_t)obj.object_index >= tensors.size())
            continue;
        GVA::Tensor *tensor = tensors[obj.object_index];
        tensor->set_string("label", gallery->GetLabelByID(obj.label));
        tensor->set_double("confidence", obj.confidence);
        tensor->set_int("label_id", obj.label + 1); // 0=Unrecognized object, recognized objects starting value 1
        tensor->set_int("object_id", obj.object_id);
    }
}

GstFlowReturn frame_to_identify(GstGvaIdentify *ovino, GstBuffer *buf, GstVideoInfo *info) {
    ovino->identifier->ProcessOutput(buf, info);
    return GST_FLOW_OK;
}

Identify *identifier_new(GstGvaIdentify *ovino) {
    return new Identify(ovino);
}

void identifier_delete(Identify *identifier) {
    delete identifier;
}
