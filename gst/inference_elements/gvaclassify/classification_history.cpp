/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "classification_history.h"

#include <gva_roi_meta.h>
#include <gva_utils.h>

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
    if (roi->id <= 0) { // object has not been tracked
        result = true;
    } else if (history.count(roi->id) == 0) { // new object
        history[roi->id].frame_of_last_update = current_num_frame;
        result = true;
    } else if (current_num_frame - history[roi->id].frame_of_last_update > gva_classify->skip_interval) {
        // new object or reclassify old object
        history[roi->id].frame_of_last_update = current_num_frame;
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
    GstVideoRegionOfInterestMeta *roi = NULL;
    void *state = NULL;
    std::lock_guard<std::mutex> guard(history_mutex);
    while ((roi = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
        if (roi->id <= 0)
            continue;
        GstStructure *roi_param = NULL;
        if (history.count(roi->id)) {
            const auto &roi_history = history[roi->id];
            if (this->current_num_frame > roi_history.frame_of_last_update) // protect from filling just classified ROIs
                for (const auto &layer_to_roi_param : roi_history.layers_to_roi_params) {
                    roi_param = gst_structure_copy(layer_to_roi_param.second);
                    gst_video_region_of_interest_meta_add_param(roi, roi_param);
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
