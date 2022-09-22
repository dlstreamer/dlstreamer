/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/gst/dictionary.h"
#include "dlstreamer/gst/utils.h"
#include "dlstreamer/image_metadata.h"

#include "dlstreamer/gst/metadata/gva_tensor_meta.h"

namespace dlstreamer {

class GSTMetadata : public Metadata {
    // TODO migrate to GstCustomMeta introduced in GStreamer 1.20, use gst_custom_meta_get_structure etc
    std::vector<DictionaryPtr> _container;
    GstBuffer *_buffer = nullptr;

  public:
    GSTMetadata(GstBuffer *buf) : _buffer(buf) {
        read_meta(buf);
    }

    GSTMetadata(GstBufferList *buffer_list) {
        for (guint i = 0; i < gst_buffer_list_length(buffer_list); i++) {
            _buffer = gst_buffer_list_get(buffer_list, i);
            read_meta(_buffer);
        }
    }

    void clear() noexcept override {
        assert(_buffer);
        GType meta_api_type = gst_gva_tensor_meta_api_get_type();
        auto remove_all = [](GstBuffer *, GstMeta **meta, gpointer user_data) -> gboolean {
            if ((*meta)->info->api == *reinterpret_cast<GType *>(user_data))
                *meta = nullptr;
            return true;
        };
        gst_buffer_foreach_meta(_buffer, remove_all, &meta_api_type);

        _container.clear();
    }

    iterator begin() noexcept override {
        return _container.begin();
    }

    iterator end() noexcept override {
        return _container.end();
    }

    DictionaryPtr add(std::string_view name) override {
        auto gst_meta = GST_GVA_TENSOR_META_ADD(_buffer);
        if (!name.empty())
            gst_structure_set_name(gst_meta->data, name.data());
        auto meta = std::make_shared<GSTDictionary>(gst_meta->data);
        _container.emplace_back(meta);
        return meta;
    }

    iterator erase(iterator pos) override {
        auto dict = ptr_cast<GSTDictionary>(*pos);
        GstGVATensorMeta *gst_meta;
        gpointer state = nullptr;
        while ((gst_meta = GST_GVA_TENSOR_META_ITERATE(_buffer, &state))) {
            if (gst_meta->data == dict->_structure) {
                break;
            }
        }

        // remove from GstBuffer
        DLS_CHECK(_buffer && gst_meta);
        gst_buffer_remove_meta(_buffer, &gst_meta->meta);

        // Remove from container
        return _container.erase(pos);
    }

  protected:
    void read_meta(GstBuffer *buf) {
        // TODO migrate to GstCustomMeta introduced in GStreamer 1.20, use gst_custom_meta_get_structure etc
        GstGVATensorMeta *custom_meta;
        gpointer state = nullptr;
        while ((custom_meta = GST_GVA_TENSOR_META_ITERATE(buf, &state))) {
            _container.emplace_back(std::make_shared<GSTDictionary>(custom_meta->data));
        }
    }
};

/////////////////////////////////////////////////////////////////////////////////////
// GSTROIDictionary

class GSTROIDictionary : public BaseDictionary {
  public:
    GSTROIDictionary(GstVideoRegionOfInterestMeta *roi, const GstVideoInfo *video_info) {
        DLS_CHECK(video_info && video_info->width != 0 && video_info->height != 0)
        _name = DetectionMetadata::name;
        _map[DetectionMetadata::key::x_min] = static_cast<double>(roi->x) / video_info->width;
        _map[DetectionMetadata::key::y_min] = static_cast<double>(roi->y) / video_info->height;
        _map[DetectionMetadata::key::x_max] = static_cast<double>(roi->x + roi->w) / video_info->width;
        _map[DetectionMetadata::key::y_max] = static_cast<double>(roi->y + roi->h) / video_info->height;
        _map[DetectionMetadata::key::id] = roi->id;
        _map[DetectionMetadata::key::parent_id] = roi->parent_id;
        auto label = g_quark_to_string(roi->roi_type);
        if (label)
            _map[DetectionMetadata::key::label] = std::string(label);
    }

    void set(std::string_view /*key*/, Any /*value*/) override {
        throw std::runtime_error("Unsupported");
    }
};

/////////////////////////////////////////////////////////////////////////////////////
// GSTROIMetadata

class GSTROIMetadata : public Metadata {
    std::vector<DictionaryPtr> _container;
    GstVideoRegionOfInterestMeta *_roi = nullptr;

  public:
    GSTROIMetadata(GstVideoRegionOfInterestMeta *roi, const GstVideoInfo *video_info) : _roi(roi) {
        // first metadata is special metadata created on GstVideoRegionOfInterestMeta structure fields
        _container.emplace_back(std::make_shared<GSTROIDictionary>(roi, video_info));
        // other metadata created on GstStructure list from GstVideoRegionOfInterestMeta params
        for (GList *l = _roi->params; l; l = g_list_next(l)) {
            GstStructure *structure = GST_STRUCTURE(l->data);
            _container.emplace_back(std::make_shared<GSTDictionary>(structure));
        }
    }

    void clear() noexcept override {
        assert(_roi);
        // FIXME: this call removes all params from ROI - need to add some filter to remove params added by DLS
        GList *params = _roi->params;
        _roi->params = nullptr;
        g_list_free_full(params, reinterpret_cast<GDestroyNotify>(gst_structure_free));

        _container.clear();
    }

    iterator begin() noexcept override {
        return _container.begin();
    }

    iterator end() noexcept override {
        return _container.end();
    }

    DictionaryPtr add(std::string_view name) override {
        GstStructure *structure = gst_structure_new_empty(name.data());
        gst_video_region_of_interest_meta_add_param(_roi, structure);
        auto item = std::make_shared<GSTDictionary>(structure);
        _container.push_back(item);
        return item;
    }

    iterator erase(iterator pos) override {
        auto dict = ptr_cast<GSTDictionary>(*pos);
        auto l = g_list_remove(_roi->params, dict->_structure);
        (void)l;
        return _container.erase(pos);
    }
};

} // namespace dlstreamer
