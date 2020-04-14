/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "classification_history.h"

#include "gva_utils.h"
#include "inference_backend/logger.h"
#include <video_frame.h>

#include <algorithm>

ClassificationHistory::ClassificationHistory(GstGvaClassify *gva_classify)
    : gva_classify(gva_classify), current_num_frame(0) {
}

bool ClassificationHistory::IsROIClassificationNeeded(GstVideoRegionOfInterestMeta *roi, unsigned current_num_frame) {
    try {
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
        } else if (gva_classify->reclassify_interval == 0) {
            return false;
        } else if (current_num_frame - history[id].frame_of_last_update >= gva_classify->reclassify_interval) {
            // new object or reclassify old object
            history[id].frame_of_last_update = current_num_frame;
            result = true;
        }
        return result;
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to check if detection tensor classification needed"));
    }
}

void ClassificationHistory::UpdateROIParams(int roi_id, GstStructure *roi_param) {
    try {
        std::lock_guard<std::mutex> guard(history_mutex);
        const gchar *layer_c = gst_structure_get_name(roi_param);
        if (not layer_c)
            throw std::runtime_error("Can't get name of region of interest param structure");
        std::string layer(layer_c);
        GstStructure **p_roi_param = &history[roi_id].layers_to_roi_params[layer];
        if (*p_roi_param)
            gst_structure_free(*p_roi_param);
        *p_roi_param = gst_structure_copy(roi_param);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to update detection tensor parameters"));
    }
}

void ClassificationHistory::FillROIParams(GstBuffer *buffer) {
    try {
        GVA::VideoFrame video_frame(buffer, gva_classify->base_inference.info);
        std::lock_guard<std::mutex> guard(history_mutex);
        for (GVA::RegionOfInterest &region : video_frame.regions()) {
            gint id = region.object_id();
            if (!id)
                continue;
            if (history.count(id)) {
                const auto &roi_history = history[id];
                int frames_ago = this->current_num_frame - roi_history.frame_of_last_update;
                for (const auto &layer_to_roi_param : roi_history.layers_to_roi_params) {
                    if (!gst_video_region_of_interest_meta_get_param(
                            region._meta(), gst_structure_get_name(layer_to_roi_param.second))) {
                        if (not region._meta())
                            throw std::logic_error(
                                "GstVideoRegionOfInterestMeta is nullptr for current region of interest");
                        auto tensor = std::unique_ptr<GstStructure, decltype(&gst_structure_free)>(
                            gst_structure_copy(layer_to_roi_param.second), gst_structure_free);
                        if (not tensor)
                            throw std::runtime_error("Failed to create classification tensor");
                        gst_structure_set(tensor.get(), "frames_ago", G_TYPE_INT, frames_ago, NULL);
                        gst_video_region_of_interest_meta_add_param(region._meta(), tensor.release());
                    }
                }
            }
        }
    } catch (const std::exception &e) {
        std::string err = "Failed to fill detection tensor parameters from history:\n" + CreateNestedErrorMsg(e);
        GVA_ERROR(err.c_str());
    }
}

ClassificationHistory *create_classification_history(GstGvaClassify *gva_classify) {
    try {
        return new ClassificationHistory(gva_classify);
    } catch (const std::exception &e) {
        GST_ELEMENT_ERROR(gva_classify, LIBRARY, INIT, ("gvaclassify intitialization failed"),
                          ("%s", CreateNestedErrorMsg(e).c_str()));
        return nullptr;
    }
}

void release_classification_history(ClassificationHistory *classification_history) {
    delete classification_history;
}

void fill_roi_params_from_history(ClassificationHistory *classification_history, GstBuffer *buffer) {
    classification_history->FillROIParams(buffer);
}
