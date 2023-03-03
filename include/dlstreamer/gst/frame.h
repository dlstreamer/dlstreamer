/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/frame.h"
#include "dlstreamer/gst/metadata.h"
#include "dlstreamer/gst/tensor.h"
#include "dlstreamer/gst/utils.h"
#include "dlstreamer/image_info.h"
#include "dlstreamer/image_metadata.h"

#include <stdexcept>

namespace dlstreamer {

class GSTFrame;
using GstFramePtr = std::shared_ptr<GSTFrame>;

class GSTFrame : public BaseFrame {
  public:
    // GstBuffer + FrameInfo
    GSTFrame(GstBuffer *buffer, const FrameInfo &info, bool take_ownership = false, ContextPtr context = nullptr)
        : BaseFrame(info.media_type, info.format, MemoryType::GST), _gst_buffer(buffer),
          _take_ownership(take_ownership) {
        init(buffer, info, context);
        _metadata = std::make_unique<GSTMetadata>(buffer);
    }
    // GstBuffer + GstVideoInfo [+ GstVideoRegionOfInterestMeta]
    GSTFrame(GstBuffer *buffer, const GstVideoInfo *video_info, GstVideoRegionOfInterestMeta *roi = nullptr,
             bool take_ownership = false, ContextPtr context = nullptr)
        : BaseFrame(MediaType::Image, 0, MemoryType::GST), _gst_buffer(buffer), _video_info(video_info),
          _take_ownership(take_ownership) {
        init(buffer, gst_video_info_to_frame_info(video_info), context);
        if (roi)
            _metadata = std::make_unique<GSTROIMetadata>(roi, video_info);
        else
            _metadata = std::make_unique<GSTMetadata>(buffer, video_info);
    }
    // GSTTensor vector
    GSTFrame(MediaType media_type, Format format, TensorVector tensors, bool take_ownership = true)
        : BaseFrame(media_type, format, tensors), _take_ownership(take_ownership) {
        _gst_buffer = gst_buffer_new();
        for (auto &tensor : tensors) {
            GstMemory *mem = ptr_cast<GSTTensor>(tensor)->gst_memory();
            gst_buffer_insert_memory(_gst_buffer, -1, gst_memory_ref(mem));
        }
        _metadata = std::make_unique<GSTMetadata>(_gst_buffer);
    }

    ~GSTFrame() {
        if (_take_ownership && _gst_buffer)
            gst_buffer_unref(_gst_buffer);
    }
    GstBuffer *gst_buffer() {
        return _gst_buffer;
    }
    operator GstBuffer *() {
        return _gst_buffer;
    }
    const GstVideoInfo *video_info() {
        return _video_info;
    }
    Metadata &metadata() override {
        return *_metadata;
    }
    std::vector<FramePtr> regions() const override {
        std::vector<FramePtr> regions;
        GstMeta *meta = NULL;
        gpointer state = NULL;
        GType meta_api_type = GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE;
        while ((meta = gst_buffer_iterate_meta_filtered(_gst_buffer, &state, meta_api_type))) {
            auto roi_meta = (GstVideoRegionOfInterestMeta *)meta;
            auto frame = std::make_shared<GSTFrame>(_gst_buffer, _video_info, roi_meta);
            auto tensor0 = ptr_cast<GSTTensor>(frame->tensor(0));
            tensor0->crop(roi_meta->x, roi_meta->y, roi_meta->w, roi_meta->h);
            auto label = g_quark_to_string(roi_meta->roi_type);
            tensor0->set_handle(DetectionMetadata::key::label, reinterpret_cast<size_t>(label)); // TODO better way?
            regions.emplace_back(frame);
        }
        return regions;
    }

  protected:
    GstBuffer *_gst_buffer = nullptr;
    const GstVideoInfo *_video_info = nullptr;
    std::unique_ptr<Metadata> _metadata;
    bool _take_ownership = false;

    void init(GstBuffer *buffer, const FrameInfo &info, ContextPtr context) {
        _media_type = info.media_type;
        _format = info.format;

        // create tensor object(s)
        if (_video_info) {
            if (gst_buffer_n_memory(buffer) != 1)
                throw std::runtime_error("Expect GstBuffer with single GstMemory");
            GstMemory *mem = gst_buffer_peek_memory(buffer, 0);
            for (size_t i = 0; i < info.tensors.size(); i++) {
                auto gst_tensor = std::make_shared<GSTTensor>(info.tensors[i], gst_memory_ref(mem), true, context, i);
                const size_t offset = GST_VIDEO_INFO_PLANE_OFFSET(_video_info, i);
                gst_tensor->set_handle(tensor::key::offset, offset);
                _tensors.emplace_back(std::move(gst_tensor));
            }
        } else {
            if (info.tensors.size()) {
                DLS_CHECK(info.tensors.size() == gst_buffer_n_memory(buffer))
            }
            for (size_t i = 0; i < info.tensors.size(); i++) {
                GstMemory *mem = gst_buffer_peek_memory(buffer, i);
                _tensors.push_back(std::make_shared<GSTTensor>(info.tensors[i], gst_memory_ref(mem), true, context));
            }
        }

        // If GstVideoCropMeta in GstBuffer, apply crop to first tensor and remove meta from GstBuffer
        GstVideoCropMeta *crop_meta = gst_buffer_get_video_crop_meta(buffer);
        if (crop_meta && !_tensors.empty()) {
            auto tensor = ptr_cast<GSTTensor>(_tensors.front());
            tensor->crop(crop_meta->x, crop_meta->y, crop_meta->width, crop_meta->height);
            gst_buffer_remove_meta(buffer, &crop_meta->meta);
        }
    }

    GSTFrame(const FrameInfo &info)
        : BaseFrame(info.media_type, info.format, MemoryType::GST), _gst_buffer(nullptr), _take_ownership(false) {
    }
};

} // namespace dlstreamer
