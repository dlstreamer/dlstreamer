/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tracker_c.h"
#include "gva_utils.h"
#include "iou/tracker.h"
#include "tracker_factory.h"

ITracker *acquire_tracker_instance(const GstVideoInfo *info, GstGvaTrackingType tracking_type, GError **error) {
    try {
        if (!info)
            throw std::invalid_argument("Could not create tracker.");

        ITracker *tracker = TrackerFactory::Create(tracking_type, info);
        if (!tracker)
            g_set_error(error, 1, 1, "Could not create tracker of %d tracking type", tracking_type);

        return tracker;
    } catch (const std::exception &e) {
        g_set_error(error, 1, 1, "%s", CreateNestedErrorMsg(e).c_str());
        return nullptr;
    }
}

void transform_tracked_objects(ITracker *tracker, GstBuffer *buffer, GError **error) {
    try {
        if (!tracker || !buffer) {
            throw std::invalid_argument("Invalid pointer");
        }
        tracker->track(buffer);
    } catch (const std::exception &e) {
        g_set_error(error, 1, 1, "%s", CreateNestedErrorMsg(e).c_str());
    }
}

void release_tracker_instance(ITracker *tracker) {
    if (tracker)
        delete tracker;
}
