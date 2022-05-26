/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/buffer_base.h"
#include "dlstreamer/fourcc.h"
#include "dlstreamer/gst/dictionary.h"
#include "dlstreamer/gst/utils.h"

#include <stdexcept>

namespace dlstreamer {

class GSTBuffer : public BufferBase {
  public:
    GSTBuffer(GstBuffer *buffer, BufferInfoCPtr info, bool take_ownership = false)
        : BufferBase(BufferType::GST_BUFFER, nullptr), _take_ownership(take_ownership) {
        init(buffer, info);
    }
    GSTBuffer(GstBuffer *buffer, const BufferInfo &info, bool take_ownership = false)
        : BufferBase(BufferType::GST_BUFFER, nullptr), _take_ownership(take_ownership) {
        init(buffer, std::make_shared<BufferInfo>(info));
    }
    GSTBuffer(GstBuffer *buffer, const GstVideoInfo *video_info, GstVideoRegionOfInterestMeta *roi = nullptr,
              bool take_ownership = false)
        : BufferBase(BufferType::GST_BUFFER, nullptr), _video_info(video_info), _roi(roi),
          _take_ownership(take_ownership) {
        init(buffer, gst_video_info_to_buffer_info(video_info));
    }
    ~GSTBuffer() {
        if (_take_ownership)
            gst_buffer_unref(_gst_buffer);
    }
    GstBuffer *gst_buffer() {
        return _gst_buffer;
    }
    const GstVideoInfo *video_info() {
        return _video_info;
    }
    DictionaryPtr add_metadata(const std::string &name) override {
        if (_roi) {
            GstStructure *structure = gst_structure_new_empty(name.c_str());
            gst_video_region_of_interest_meta_add_param(_roi, structure);
            auto meta_ptr = std::make_shared<GSTDictionary>(structure);
            _metadata.push_back(meta_ptr);
            return meta_ptr;
        } else {
            const GstMetaInfo *meta_info = gst_meta_get_info(CUSTOM_META_NAME);
            if (!meta_info)
                throw std::runtime_error("Meta info not found: " + std::string(CUSTOM_META_NAME));
            if (!gst_buffer_is_writable(_gst_buffer))
                throw std::runtime_error("add_metadata() called on non-writable GstBuffer");
            auto *custom_meta =
                reinterpret_cast<_GstGVACustomMeta *>(gst_buffer_add_meta(_gst_buffer, meta_info, nullptr));
            if (!custom_meta)
                throw std::runtime_error("Error adding custom meta");
            auto meta_ptr = std::make_shared<GSTDictionary>(custom_meta->structure);
            meta_ptr->set_name(name);
            _metadata.push_back(meta_ptr);
            return meta_ptr;
        }
    }
    void remove_metadata(DictionaryPtr meta) override {
        // remote from GstBuffer
        auto gst_meta = std::dynamic_pointer_cast<GSTDictionary>(meta);
        if (!gst_meta)
            throw std::runtime_error("Error casting to GSTDictionary");
        GstMeta *custom_meta = NULL;
        gpointer state = NULL;
        GType meta_api_type = g_type_from_name(CUSTOM_META_API_NAME);
        while ((custom_meta = gst_buffer_iterate_meta_filtered(_gst_buffer, &state, meta_api_type))) {
            if (reinterpret_cast<_GstGVACustomMeta *>(custom_meta)->structure == gst_meta->_structure) {
                gst_buffer_remove_meta(_gst_buffer, custom_meta);
                break;
            }
        }
        if (custom_meta == NULL) {
            throw std::runtime_error("Meta not found in GstBuffer");
        }
        // remove from this object
        auto it = std::find(_metadata.begin(), _metadata.end(), meta);
        if (it == _metadata.end())
            throw std::runtime_error("Meta not found");
        _metadata.erase(it);
    }
    std::vector<std::string> keys() const override {
        return {pts_id};
    }

  protected:
    GstBuffer *_gst_buffer = nullptr;
    const GstVideoInfo *_video_info = nullptr;
    GstVideoRegionOfInterestMeta *_roi = nullptr;
    bool _take_ownership = false;
    // TODO migrate to GstCustomMeta introduced in GStreamer 1.20, use gst_custom_meta_get_structure etc
    static constexpr auto CUSTOM_META_NAME = "GstGVATensorMeta";
    static constexpr auto CUSTOM_META_API_NAME = "GstGVATensorMetaAPI";
    struct _GstGVACustomMeta {
        GstMeta meta;
        GstStructure *structure;
    };

    void init(GstBuffer *buffer, BufferInfoCPtr info) {
        _gst_buffer = buffer;
        _info = info;
        // PTS
        set_handle(pts_id, 0, static_cast<intptr_t>(GST_BUFFER_PTS(_gst_buffer)));
        // metadata
        read_metadata();
    }
    void read_metadata() {
        if (_roi) {
            for (GList *l = _roi->params; l; l = g_list_next(l)) {
                GstStructure *structure = GST_STRUCTURE(l->data);
                _metadata.push_back(std::make_shared<GSTDictionary>(structure));
            }
        } else {
            GstMeta *meta = NULL;
            gpointer state = NULL;
            GType meta_api_type = g_type_from_name(CUSTOM_META_API_NAME);
            while ((meta = gst_buffer_iterate_meta_filtered(_gst_buffer, &state, meta_api_type))) {
                auto *custom_meta = reinterpret_cast<_GstGVACustomMeta *>(meta);
                _metadata.push_back(std::make_shared<GSTDictionary>(custom_meta->structure));
            }
        }
    }
};

using GSTBufferPtr = std::shared_ptr<GSTBuffer>;

} // namespace dlstreamer
