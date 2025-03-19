/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/gst/dictionary.h"
#include "dlstreamer/gst/utils.h"

#include "dlstreamer/gst/metadata/gva_tensor_meta.h"

namespace dlstreamer {

class GSTMetadata : public Metadata {
    // TODO migrate to GstCustomMeta introduced in GStreamer 1.20, use gst_custom_meta_get_structure etc
    std::vector<DictionaryPtr> _container;
    GstBuffer *_buffer = nullptr;
    const GstVideoInfo *_video_info = nullptr;

  public:
    GSTMetadata(GstBuffer *buf, const GstVideoInfo *video_info = nullptr) : _buffer(buf), _video_info(video_info) {
        read_meta(buf);
    }

    GSTMetadata(GstBufferList *buffer_list) {
        for (guint i = 0; i < gst_buffer_list_length(buffer_list); i++) {
            _buffer = gst_buffer_list_get(buffer_list, i);
            read_meta(_buffer);
        }
    }

    DictionaryPtr find_metadata(std::string_view meta_name) {
        for (auto &meta : _container) {
            if (meta->name() == meta_name)
                return meta;
        }
        return nullptr;
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
        DictionaryPtr dict;

        if (_video_info && name == DetectionMetadata::name) {
            // GstVideoRegionOfInterestMeta + GstStruct
            auto roi_meta = gst_buffer_add_video_region_of_interest_meta(_buffer, NULL, 0, 0, 0, 0);
            auto roi_struct = gst_structure_new_empty(DetectionMetadata::name);
            gst_video_region_of_interest_meta_add_param(roi_meta, roi_struct);
            dict = std::make_shared<GSTROIDictionary>(roi_meta, _video_info->width, _video_info->height, roi_struct);
        } else {
            auto gst_meta = GST_GVA_TENSOR_META_ADD(_buffer);
            if (!name.empty())
                gst_structure_set_name(gst_meta->data, name.data());
            dict = std::make_shared<GSTDictionary>(gst_meta->data);
        }
        _container.emplace_back(dict);
        return dict;
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
// GSTROIMetadata

class GSTROIMetadata : public Metadata {
    std::vector<DictionaryPtr> _container;
    GstVideoRegionOfInterestMeta *_roi = nullptr;

  public:
    GSTROIMetadata(GstVideoRegionOfInterestMeta *roi, const GstVideoInfo *video_info) : _roi(roi) {
        // iterate GstStructure list in GstVideoRegionOfInterestMeta params
        for (GList *l = _roi->params; l; l = g_list_next(l)) {
            DictionaryPtr dict;
            GstStructure *structure = GST_STRUCTURE(l->data);
            std::string name = gst_structure_get_name(structure);
            if (name == DetectionMetadata::name) {
                // special metadata created on GstVideoRegionOfInterestMeta plus GstStructure with norm. coordinates
                dict = std::make_shared<GSTROIDictionary>(roi, video_info->width, video_info->height, structure);
            } else {
                dict = std::make_shared<GSTDictionary>(structure);
            }
            _container.emplace_back(dict);
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
