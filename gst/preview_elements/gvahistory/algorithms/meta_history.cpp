/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "meta_history.hpp"

#include <gva_roi_ref_meta.hpp>
#include <meta/gva_buffer_flags.hpp>

#include <gva_utils.h>

namespace {
/* TODO: workaround to use our API without VideoInfo */
auto video_info =
    std::unique_ptr<GstVideoInfo, std::function<void(GstVideoInfo *)>>(gst_video_info_new(), gst_video_info_free);
} // namespace

MetaHistory::MetaHistory(size_t interval) : _interval(interval), _history(CLASSIFICATION_HISTORY_SIZE) {
}

bool MetaHistory::invoke(GstBuffer *buffer) {
    std::lock_guard<std::mutex> guard(_mutex);

    const GVA::VideoFrame frame(buffer, video_info.get());
    bool ret = false;
    for (const auto &roi : frame.regions()) {
        // If we need history update for any ROI then we need to push buffer on inference
        if (need_update(roi._meta())) {
            ret = true;
            break;
        }
    }

    // TODO: assume that we have ROIs and flag on last ROI is set
    if (gst_buffer_has_flags(buffer, static_cast<GstBufferFlags>(GVA_BUFFER_FLAG_LAST_ROI_ON_FRAME)))
        _frame_num++;
    return ret;
}

void MetaHistory::save(GstBuffer *buffer) {
    if (_interval == 1) {
        return;
    }

    std::lock_guard<std::mutex> guard(_mutex);

    const GVA::VideoFrame frame(buffer, video_info.get());
    // TODO: support history for detection?

    auto object_id = -1;
    auto roi_ref_meta = GVA_ROI_REF_META_GET(buffer);
    if (roi_ref_meta) {
        object_id = roi_ref_meta->object_id;
        if (object_id > 0) {
            for (const auto &tensor : frame.tensors()) {
                internal_save(object_id, tensor.gst_structure());
            }
        }
    }
}

void MetaHistory::fill(GstBuffer *buffer) {
    std::lock_guard<std::mutex> guard(_mutex);

    GVA::VideoFrame frame(buffer, video_info.get());
    for (auto &region : frame.regions()) {
        internal_fill(region, buffer);
    }
}

bool MetaHistory::need_update(GstVideoRegionOfInterestMeta *meta) {
    gint id;
    if (!get_object_id(meta, &id)) {
        /* object has not been tracked */
        return true;
    }
    if (_history.count(id) == 0) {
        /* new object in _history */
        _history.put(id);
        _history.get(id).last_update_frame = _frame_num;
        return true;
    } else if (_interval == 0) {
        /* interval property is not set */
        return false;
    } else {
        uint64_t last_update = _history.get(id).last_update_frame;
        auto current_interval = _frame_num - last_update;
        if (current_interval > INT64_MAX && last_update > _frame_num)
            current_interval = (UINT64_MAX - last_update) + _frame_num + 1;
        if (current_interval >= _interval) {
            /* new object or reclassify old object */
            _history.get(id).last_update_frame = _frame_num;
            return true;
        }
    }

    return false;
}

void MetaHistory::internal_save(int roi_id, const GstStructure *roi_param) {
    const gchar *layer_c = gst_structure_get_name(roi_param);
    if (!layer_c) {
        throw std::runtime_error("Can't get region of interest param structure name.");
    }
    std::string layer(layer_c);

    // To prevent attempts to access removed objects,
    // we should read lost objects to _history if needed
    validate(roi_id);

    _history.get(roi_id).layers_to_roi_params[layer] =
        GstStructureSharedPtr(gst_structure_copy(roi_param), gst_structure_free);
}

void MetaHistory::internal_fill(GVA::RegionOfInterest &region, GstBuffer *buffer) {
    int32_t id = region.object_id();
    g_assert(id > 0 && "Untracked object received in history");

    if (_history.count(id) == 0)
        return;

    const auto &roi_history = _history.get(id);
    // int frames_ago = _frame_num - roi_history.last_update_frame;
    for (const auto &layer_to_roi_param : roi_history.layers_to_roi_params) {
        if (gst_video_region_of_interest_meta_get_param(region._meta(),
                                                        gst_structure_get_name(layer_to_roi_param.second.get())))
            continue;

        if (not region._meta())
            throw std::logic_error("GstVideoRegionOfInterestMeta is nullptr for current region of interest");
        auto tensor = GstStructureUniquePtr(gst_structure_copy(layer_to_roi_param.second.get()), gst_structure_free);
        if (not tensor)
            throw std::runtime_error("Failed to create classification tensor");

        auto tensor_meta = GST_GVA_TENSOR_META_ADD(buffer);
        if (tensor_meta->data) {
            gst_structure_free(tensor_meta->data);
        }
        tensor_meta->data = tensor.release();
        /* TODO: seems unused */
        // gst_structure_set(tensor.get(), "frames_ago", G_TYPE_INT, frames_ago, NULL);
    }

    // Add ROI ref meta and remove ROI
    auto roi_ref_meta = GVA_ROI_REF_META_ADD(buffer);
    roi_ref_meta->reference_roi_id = region._meta()->id;
    roi_ref_meta->object_id = id;
    gst_buffer_remove_meta(buffer, GST_META_CAST(region._meta()));
}

void MetaHistory::validate(int roi_id) {
    if (_history.count(roi_id) == 0) {
        _history.put(roi_id);
        _history.get(roi_id).last_update_frame = _frame_num;
    }
}
