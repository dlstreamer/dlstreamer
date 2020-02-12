/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <stdio.h>

#include "converters.h"
#include "gstgvametaconvert.h"
#include "gva_json_meta.h"
#include "gva_tensor_meta.h"
#include "video_frame.h"

#include "gva_utils.h"

#define UNUSED(x) (void)(x)

int check_model_and_layer_name(GstStructure *s, gchar *model_name, gchar *layer_name) {
    if (model_name) {
        const gchar *s_model_name = gst_structure_get_string(s, "model_name");
        if (!s_model_name || !g_strrstr(model_name, s_model_name))
            return 0;
    }
    if (layer_name) {
        const gchar *s_layer_name = gst_structure_get_string(s, "layer_name");
        if (!s_layer_name || !g_strrstr(layer_name, s_layer_name))
            return 0;
    }
    return 1;
}

gboolean to_json(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    if (converter == nullptr) {
        GST_ERROR("GVA meta convert data pointer is null");
        return FALSE;
    }

    switch (converter->method) {
    case GST_GVA_METACONVERT_ALL:
        all_to_json(converter, buffer);
        break;
    case GST_GVA_METACONVERT_DETECTION:
        detection_to_json(converter, buffer);
        break;
    case GST_GVA_METACONVERT_TENSOR:
        tensor_to_json(converter, buffer);
        break;
    default:
        GST_DEBUG_OBJECT(converter, "Invalid method input");
        return FALSE;
    }
    return TRUE;
}

gboolean dump_detection(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    if (converter == nullptr) {
        GST_ERROR("GVA meta convert data pointer is null");
        return FALSE;
    }

    GstVideoRegionOfInterestMeta *meta = nullptr;
    GVA::VideoFrame video_frame(buffer, converter->info);
    for (GVA::RegionOfInterest &roi : video_frame.regions()) {
        meta = roi.meta();
        gint id = 0;
        get_object_id(meta, &id);
        GST_INFO("Detection: "
                 "id: %d, x: %d, y: %d, w: %d, h: %d, roi_type: %s",
                 id, meta->x, meta->y, meta->w, meta->h, g_quark_to_string(meta->roi_type));
    }
    return TRUE;
}

gboolean dump_classification(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    if (converter == nullptr) {
        GST_ERROR("GVA meta convert data pointer is null");
        return FALSE;
    }

    GstVideoRegionOfInterestMeta *meta = nullptr;
    GVA::VideoFrame video_frame(buffer, converter->info);
    for (GVA::RegionOfInterest &roi : video_frame.regions()) {
        meta = roi.meta();
        gint id = 0;
        get_object_id(meta, &id);
        for (GVA::Tensor &tensor : roi) {
            GST_DEBUG("Classification:\n\tmeta_id %d\n\tlabel %s", id, tensor.label().c_str());
        }
    }
    return TRUE;
}

gboolean dump_tensors(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    if (converter == nullptr) {
        GST_ERROR("GVA meta convert data pointer is null");
        return FALSE;
    }

    GVA::VideoFrame video_frame(buffer, converter->info);
    constexpr gint max_printed_data_bytes = 32;
    GST_DEBUG("Dump tensors: %s %s", converter->inference_id, converter->layer_name);
    for (GVA::Tensor &tensor : video_frame.tensors()) {
        if ((converter->inference_id and tensor.element_id() != converter->inference_id) or
            (converter->layer_name and tensor.layer_name() != converter->layer_name))
            continue;

        char buffer[256] = {0};
        std::vector<unsigned char> data = tensor.data<unsigned char>();
        for (gint i = 0; i < max_printed_data_bytes && i < tensor.total_bytes(); i++) {
            g_snprintf(buffer + i * 6, sizeof(buffer), "0x%02x, ", data[i]);
        }

        std::vector<guint> dims = tensor.dims();
        if (dims.size() < 4) {
            GST_ERROR("The dims array size is smaller than expected");
            return FALSE;
        }

        GST_INFO("Tensor:\n"
                 "\t inference_id: %s\n"
                 "\t data_size: %d\n"
                 "\t dims number: %d\n"
                 "\t layer name: %s\n"
                 "\t model: %s\n"
                 "\t dims: %d, %d, %d, %d\n"
                 "\t data: { %s... }\n",
                 tensor.element_id().c_str(), tensor.total_bytes(), tensor.rank(), tensor.layer_name().c_str(),
                 tensor.model_name().c_str(), dims[0], dims[1], dims[2], dims[3], buffer);
    }
    return TRUE;
}

gboolean tensor2text(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    if (converter == nullptr) {
        GST_ERROR("GVA meta convert data pointer is null");
        return FALSE;
    }

    GVA::VideoFrame video_frame(buffer, converter->info);
    for (GVA::RegionOfInterest &roi : video_frame.regions()) {
        for (GVA::Tensor &tensor : roi) {
            if (!check_model_and_layer_name(tensor.gst_structure(), converter->model, converter->layer_name))
                continue;
            std::vector<float> data = tensor.data<float>();
            if (data.size() == 0)
                continue;
            char buff[1024] = {0};
            snprintf(buff, sizeof(buff), "%.2f", data[0]);
            tensor.set_string("label", buff);
        }
    }
    return TRUE;
}

gboolean tensors_to_file(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    if (converter == nullptr) {
        GST_ERROR("GVA meta convert data pointer is null");
        return FALSE;
    }
    GstVideoRegionOfInterestMeta *meta = NULL;
    gpointer state = NULL;
    static guint frame_num = 0;
    guint index = 0;
    while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
        for (GList *l = meta->params; l; l = g_list_next(l)) {
            GstStructure *s = (GstStructure *)l->data;
            if (!check_model_and_layer_name(s, converter->model, converter->layer_name))
                continue;
            gsize nbytes = 0;
            const float *data = static_cast<const float *>(gva_get_tensor_data(s, &nbytes));
            if (!data)
                continue;
            char filename[PATH_MAX] = {0};
            g_snprintf(filename, sizeof(filename), "%s/%s_frame_%u_idx_%u.tensor", converter->location,
                       converter->tags ? converter->tags : "default", frame_num, index);
            FILE *f = fopen(filename, "wb");
            if (f) {
                fwrite(data, sizeof(float), nbytes / sizeof(float), f);
                fclose(f);
            } else {
                GST_WARNING("Failed to open/create file: %s\n", filename);
            }
            index++;
        }
    }
    frame_num++;
    return TRUE;
}

gboolean add_fullframe_roi(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    if (converter == nullptr) {
        GST_ERROR("GVA meta convert data pointer is null");
        return FALSE;
    }
    GstVideoInfo *info = converter->info;
    GVA::VideoFrame video_frame(buffer, info);
    video_frame.add_region(0, 0, info->width, info->height, 0);
    return TRUE;
}

GHashTable *get_converters() {
    GHashTable *converters = g_hash_table_new(g_direct_hash, g_direct_equal);

    g_hash_table_insert(converters, GINT_TO_POINTER(GST_GVA_METACONVERT_TENSOR2TEXT), (gpointer)tensor2text);
    g_hash_table_insert(converters, GINT_TO_POINTER(GST_GVA_METACONVERT_JSON), (gpointer)to_json);
    g_hash_table_insert(converters, GINT_TO_POINTER(GST_GVA_METACONVERT_DUMP_DETECTION), (gpointer)dump_detection);
    g_hash_table_insert(converters, GINT_TO_POINTER(GST_GVA_METACONVERT_DUMP_CLASSIFICATION),
                        (gpointer)dump_classification);
    g_hash_table_insert(converters, GINT_TO_POINTER(GST_GVA_METACONVERT_DUMP_TENSORS), (gpointer)dump_tensors);
    g_hash_table_insert(converters, GINT_TO_POINTER(GST_GVA_METACONVERT_TENSORS_TO_FILE), (gpointer)tensors_to_file);
    g_hash_table_insert(converters, GINT_TO_POINTER(GST_GVA_METACONVERT_ADD_FULL_FRAME_ROI),
                        (gpointer)add_fullframe_roi);

    return converters;
}
