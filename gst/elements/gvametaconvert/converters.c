/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "converters.h"
#include "gstgvametaconvert.h"
#include <stdio.h>

#include "gva_json_meta.h"
#include "gva_roi_meta.h"
#include "gva_tensor_meta.h"

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

void to_json(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    if (converter->method == GST_GVA_METACONVERT_ALL) {
        all_to_json(converter, buffer);
    } else if (converter->method == GST_GVA_METACONVERT_DETECTION) {
        detection_to_json(converter, buffer);
    } else if (converter->method == GST_GVA_METACONVERT_TENSOR) {
        tensor_to_json(converter, buffer);
    } else {
        GST_DEBUG_OBJECT(converter, "Invalid method input");
    }
}

void dump_detection(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    UNUSED(converter);

    GstVideoRegionOfInterestMeta *meta = NULL;
    gpointer state = NULL;
    while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
        GST_INFO("Detection: "
                 "id: %d, x: %d, y: %d, w: %d, h: %d, roi_type: %s",
                 meta->id, meta->x, meta->y, meta->w, meta->h, g_quark_to_string(meta->roi_type));
    }
}

void dump_classification(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    UNUSED(converter);

    GstVideoRegionOfInterestMeta *meta = NULL;
    gpointer state = NULL;
    while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
        for (GList *l = meta->params; l; l = g_list_next(l)) {
            GstStructure *s = (GstStructure *)l->data;
            GST_DEBUG("Classification:\n\tmeta_id %d\n\tlabel %s", meta->id, gst_structure_get_string(s, "label"));
        }
    }
}

void dump_tensors(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    GstGVATensorMeta *meta = NULL;
    gpointer state = NULL;
    const guint max_printed_data_bytes = 32;
    GST_DEBUG("Dump tensors: %s %s", converter->inference_id, converter->layer_name);
    while ((meta = (GstGVATensorMeta *)gst_buffer_iterate_meta(buffer, &state))) {
        if (meta->meta.info->api != gst_gva_tensor_meta_api_get_type())
            continue;
        if (converter->inference_id && g_strcmp0(converter->inference_id, meta->element_id) != 0)
            continue;
        if (converter->layer_name && g_strcmp0(converter->layer_name, meta->layer_name) != 0)
            continue;
        char buffer[256] = {0};
        for (guint i = 0; i < max_printed_data_bytes && i < gva_tensor_size(meta); i++) {
            g_snprintf(buffer + i * 6, sizeof(buffer), "0x%02x, ", ((unsigned char *)meta->data)[i]);
        }

        GST_INFO("Tensor:\n"
                 "\t inference_id: %s\n"
                 "\t data_size: %zu\n"
                 "\t number_elements: %d\n"
                 "\t dims number: %d\n"
                 "\t layer name: %s\n"
                 "\t model: %s\n"
                 "\t dims: %zu,%zu,%zu,%zu,%zu\n"
                 "\t data: { %s... }\n",
                 meta->element_id, meta->total_bytes, gva_tensor_size(meta), meta->rank, meta->layer_name,
                 meta->model_name, meta->dims[0], meta->dims[1], meta->dims[2], meta->dims[3], meta->dims[4], buffer);
    }
}

void tensors_to_file(GstGvaMetaConvert *converter, GstBuffer *buffer) {
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
            const float *data = gva_get_tensor_data(s, &nbytes);
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
}

void tensor2text(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    GstVideoRegionOfInterestMeta *meta = NULL;
    gpointer state = NULL;
    while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
        for (GList *l = meta->params; l; l = g_list_next(l)) {
            GstStructure *s = (GstStructure *)l->data;
            if (!check_model_and_layer_name(s, converter->model, converter->layer_name))
                continue;
            gsize nbytes = 0;
            const float *data = gva_get_tensor_data(s, &nbytes);
            if (!data)
                continue;
            char buff[1024] = {0};
            snprintf(buff, sizeof(buff), "%.2f", data[0]);
            gst_structure_set(s, "label", G_TYPE_STRING, buff, NULL);
        }
    }
}

void add_fullframe_roi(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    GstVideoInfo *info = converter->info;
    gst_buffer_add_video_region_of_interest_meta(buffer, "full_frame", 0, 0, info->width, info->height);
}

GHashTable *get_converters() {
    GHashTable *converters = g_hash_table_new(g_direct_hash, g_direct_equal);

    g_hash_table_insert(converters, GINT_TO_POINTER(GST_GVA_METACONVERT_TENSOR2TEXT), tensor2text);
    g_hash_table_insert(converters, GINT_TO_POINTER(GST_GVA_METACONVERT_JSON), to_json);
    g_hash_table_insert(converters, GINT_TO_POINTER(GST_GVA_METACONVERT_DUMP_DETECTION), dump_detection);
    g_hash_table_insert(converters, GINT_TO_POINTER(GST_GVA_METACONVERT_DUMP_CLASSIFICATION), dump_classification);
    g_hash_table_insert(converters, GINT_TO_POINTER(GST_GVA_METACONVERT_DUMP_TENSORS), dump_tensors);
    g_hash_table_insert(converters, GINT_TO_POINTER(GST_GVA_METACONVERT_TENSORS_TO_FILE), tensors_to_file);
    g_hash_table_insert(converters, GINT_TO_POINTER(GST_GVA_METACONVERT_ADD_FULL_FRAME_ROI), add_fullframe_roi);

    return converters;
}
