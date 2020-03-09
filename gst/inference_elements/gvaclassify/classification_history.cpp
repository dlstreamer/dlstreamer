/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "classification_history.h"

#include "gva_utils.h"
#include <video_frame.h>

#include <algorithm>

ClassificationHistory::ClassificationHistory(GstGvaClassify *gva_classify)
    : gva_classify(gva_classify), current_num_frame(0) {
}

bool ClassificationHistory::IsROIClassificationNeeded(GstVideoRegionOfInterestMeta *roi, unsigned current_num_frame) {
    std::lock_guard<std::mutex> guard(history_mutex);
    this->current_num_frame = current_num_frame;

    // by default we assume that
    // we have recent classification result or classification is not required for this object
    bool result = false;
    gint id;
    if (!get_object_id(roi, &id))
        // object has not been tracked
        return true;
    if (history.count(id) == 0) { // new object
        history[id].frame_of_last_update = current_num_frame;
        result = true;
    } else if (current_num_frame - history[id].frame_of_last_update > gva_classify->skip_interval) {
        // new object or reclassify old object
        history[id].frame_of_last_update = current_num_frame;
        result = true;
    }
    return result;
}

void ClassificationHistory::UpdateROIParams(int roi_id, GstStructure *roi_param) {
    std::lock_guard<std::mutex> guard(history_mutex);
    std::string layer = gst_structure_get_name(roi_param);
    GstStructure **p_roi_param = &history[roi_id].layers_to_roi_params[layer];
    if (*p_roi_param)
        gst_structure_free(*p_roi_param);
    *p_roi_param = gst_structure_copy(roi_param);
}

void ClassificationHistory::FillROIParams(GstBuffer *buffer) {
    GVA::VideoFrame video_frame(buffer, gva_classify->base_inference.info);
    std::lock_guard<std::mutex> guard(history_mutex);
    for (GVA::RegionOfInterest &region : video_frame.regions()) {
        gint id;
        if (!get_object_id(region.meta(), &id))
            continue;
        if (history.count(id)) {
            const auto &roi_history = history[id];
            if (this->current_num_frame >
                roi_history.frame_of_last_update) { // protect from filling just classified ROIs
                int frames_ago = this->current_num_frame - roi_history.frame_of_last_update;
                for (const auto &layer_to_roi_param : roi_history.layers_to_roi_params) {
                    GVA::Tensor tensor = region.add_tensor(gst_structure_copy(layer_to_roi_param.second));
                    tensor.set_int("frames_ago", frames_ago);
                }
            }
        }
    }
}

ClassificationHistory *create_classification_history(GstGvaClassify *gva_classify) {
    return new ClassificationHistory(gva_classify);
}

void release_classification_history(ClassificationHistory *classification_history) {
    delete classification_history;
}

void fill_roi_params_from_history(ClassificationHistory *classification_history, GstBuffer *buffer) {
    classification_history->FillROIParams(buffer);
}
