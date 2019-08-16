/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "gstgvaclassify.h"

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

struct ClassificationHistory;
struct ClassificationHistory *create_classification_history(GstGvaClassify *gva_classify);
void release_classification_history(struct ClassificationHistory *classification_history);
void fill_roi_params_from_history(struct ClassificationHistory *classification_history, GstBuffer *buffer);

G_END_DECLS

#ifdef __cplusplus
#include <map>
#include <mutex>

struct ClassificationHistory {
    struct ROIClassificationHistory {
        unsigned frame_of_last_update;
        std::map<std::string, GstStructure *> layers_to_roi_params;

        ~ROIClassificationHistory() {
            for (auto l : layers_to_roi_params)
                gst_structure_free(l.second);
        }
    };

    GstGvaClassify *gva_classify;
    unsigned current_num_frame;
    std::map<int, ROIClassificationHistory> history;
    std::mutex history_mutex;

    ClassificationHistory(GstGvaClassify *gva_classify);

    bool IsROIClassificationNeeded(GstVideoRegionOfInterestMeta *roi, unsigned current_num_frame);
    void UpdateROIParams(int roi_id, GstStructure *roi_param);
    void FillROIParams(GstBuffer *buffer);
};
#endif
