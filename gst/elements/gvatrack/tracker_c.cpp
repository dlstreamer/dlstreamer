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
    ITracker *tracker = nullptr;
    try {
        if (!info)
            throw std::invalid_argument("Failed to create tracker");

        tracker = TrackerFactory::Create(tracking_type, info);
        if (!tracker)
            throw std::runtime_error("Failed to create tracker of " + std::to_string(tracking_type) + " tracking type");
    } catch (const std::exception &e) {
        g_set_error(error, 1, 1, "%s", CreateNestedErrorMsg(e).c_str());
    }
    return tracker;
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
    try {
        delete tracker;
    } catch (const std::exception &e) {
        GST_ERROR("%s", e.what());
    }
}
