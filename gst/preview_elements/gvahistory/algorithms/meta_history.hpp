/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "ihistory.hpp"

#include <lru_cache.h>
#include <video_frame.h>

#include <gst/video/video.h>

#include <map>
#include <memory>
#include <mutex>

constexpr size_t CLASSIFICATION_HISTORY_SIZE = 100;

/* TODO: need to move in some common file to use it project wide */
using GstStructureSharedPtr = std::shared_ptr<GstStructure>;
using GstStructureUniquePtr = std::unique_ptr<GstStructure, std::function<void(GstStructure *)>>;

class MetaHistory : public IHistory {
  private:
    struct ROIHistory {
        uint64_t last_update_frame;
        std::map<std::string, GstStructureSharedPtr> layers_to_roi_params;

        ROIHistory(uint64_t last_update_frame = {},
                   std::map<std::string, GstStructureSharedPtr> layers_to_roi_params = {})
            : last_update_frame(last_update_frame), layers_to_roi_params(layers_to_roi_params) {
        }
    };

  public:
    MetaHistory(size_t interval);
    ~MetaHistory() override = default;

    bool invoke(GstBuffer *) override;
    void save(GstBuffer *);
    void fill(GstBuffer *);

  private:
    uint64_t _frame_num = 0;
    const size_t _interval;
    LRUCache<int, ROIHistory> _history;
    std::mutex _mutex;

    bool need_update(GstVideoRegionOfInterestMeta *);
    void internal_save(int roi_id, const GstStructure *roi_param);
    void internal_fill(GVA::RegionOfInterest &object_id, GstBuffer *buffer);
    void validate(int roi_id);
};
