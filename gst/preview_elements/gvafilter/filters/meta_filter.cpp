/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "meta_filter.hpp"

#include <utils.h>

#include <gst/video/video.h>

namespace {
gboolean filter_roi_meta(GstBuffer *, GstMeta **meta, gpointer user_data) {
    if ((*meta)->info->api != GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE)
        return true;

    auto roi_meta = reinterpret_cast<GstVideoRegionOfInterestMeta *>(*meta);
    auto object_classes = static_cast<std::set<std::string> *>(user_data);
    g_assert(object_classes && "Expected object_classes to be provided with user_data");
    std::string roi_type = roi_meta->roi_type ? g_quark_to_string(roi_meta->roi_type) : std::string();

    if (object_classes->find(roi_type) == object_classes->end())
        *meta = nullptr;

    return true;
}
} // namespace

MetaFilter::MetaFilter(const std::string &object_class_filter) {
    Utils::splitString(object_class_filter, std::inserter(_object_classes, _object_classes.begin()));
}

void MetaFilter::invoke(GstBuffer *buffer) {
    if (_object_classes.empty())
        return;

    gst_buffer_foreach_meta(buffer, filter_roi_meta, &_object_classes);
}
