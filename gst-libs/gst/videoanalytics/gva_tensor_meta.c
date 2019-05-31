/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <string.h>

#include "gva_tensor_meta.h"

#define UNUSED(x) (void)(x)

GType gst_gva_tensor_meta_api_get_type(void) {
    static volatile GType type;
    static const gchar *tags[] = {GVA_TENSOR_META_TAG, NULL};

    if (g_once_init_enter(&type)) {
        GType _type = gst_meta_api_type_register("GstGVATensorMetaAPI", tags);
        g_once_init_leave(&type, _type);
    }
    return type;
}

gboolean gst_gva_tensor_meta_init(GstMeta *meta, gpointer params, GstBuffer *buffer) {
    UNUSED(params);
    UNUSED(buffer);

    GstGVATensorMeta *tensor_meta = (GstGVATensorMeta *)meta;
    tensor_meta->precision = 0;
    tensor_meta->rank = 0;
    memset(&tensor_meta->dims, 0, sizeof(tensor_meta->dims));
    tensor_meta->layout = 0;
    tensor_meta->model_name = 0;
    tensor_meta->layer_name = 0;
    tensor_meta->data = NULL;
    tensor_meta->total_bytes = 0;
    tensor_meta->element_id = NULL;
    return TRUE;
}

gboolean gst_gva_tensor_meta_transform(GstBuffer *dest_buf, GstMeta *src_meta, GstBuffer *src_buf, GQuark type,
                                       gpointer data) {
    UNUSED(src_buf);
    UNUSED(type);
    UNUSED(data);

    GstGVATensorMeta *dst = GST_GVA_TENSOR_META_ADD(dest_buf);
    GstGVATensorMeta *src = (GstGVATensorMeta *)src_meta;

    dst->precision = src->precision;
    dst->rank = src->rank;
    memcpy(dst->dims, src->dims, sizeof(src->dims));
    dst->layout = src->layout;
    dst->model_name = g_strdup(src->model_name);
    dst->layer_name = g_strdup(src->layer_name);
    dst->data = g_slice_copy(src->total_bytes, src->data);
    dst->total_bytes = src->total_bytes;
    dst->element_id = src->element_id;
    return TRUE;
}

void gst_gva_tensor_meta_free(GstMeta *meta, GstBuffer *buffer) {
    UNUSED(buffer);

    GstGVATensorMeta *tensor_meta = (GstGVATensorMeta *)meta;
    if (tensor_meta->model_name) {
        g_free(tensor_meta->model_name);
        tensor_meta->model_name = NULL;
    }
    if (tensor_meta->layer_name) {
        g_free(tensor_meta->layer_name);
        tensor_meta->layer_name = NULL;
    }
    if (tensor_meta->data) {
        g_slice_free1(tensor_meta->total_bytes, tensor_meta->data);
        tensor_meta->data = NULL;
        tensor_meta->total_bytes = 0;
    }
}

const GstMetaInfo *gst_gva_tensor_meta_get_info(void) {
    static const GstMetaInfo *meta_info = NULL;

    if (g_once_init_enter(&meta_info)) {
        const GstMetaInfo *meta = gst_meta_register(
            gst_gva_tensor_meta_api_get_type(), "GstGVATensorMeta", sizeof(GstGVATensorMeta),
            (GstMetaInitFunction)gst_gva_tensor_meta_init, (GstMetaFreeFunction)gst_gva_tensor_meta_free,
            (GstMetaTransformFunction)gst_gva_tensor_meta_transform);
        g_once_init_leave(&meta_info, meta);
    }
    return meta_info;
}

GstGVATensorMeta *find_tensor_meta_ext(GstBuffer *buffer, const char *model_name, const char *output_layer,
                                       const char *element_id) {
    GstGVATensorMeta *meta = NULL;
    gpointer state = NULL;

    if (!model_name && !output_layer && !element_id) {
        GST_WARNING("No valid arguments: model_name output_layer element_id");
        return NULL;
    }

    while ((meta = (GstGVATensorMeta *)gst_buffer_iterate_meta(buffer, &state))) {
        if (meta->meta.info->api != gst_gva_tensor_meta_api_get_type())
            continue;
        if (model_name) {
            if (!meta->model_name || strstr(meta->model_name, model_name) == 0)
                continue;
        }
        if (output_layer) {
            if (!meta->layer_name || strstr(meta->layer_name, output_layer) == 0)
                continue;
        }
        if (element_id) {
            if (!meta->element_id || strstr(meta->element_id, element_id) == 0)
                continue;
        }
        return meta;
    }

    return NULL;
}

GstGVATensorMeta *find_tensor_meta(GstBuffer *buffer, const char *model_name, const char *output_layer) {
    return find_tensor_meta_ext(buffer, model_name, output_layer, NULL);
}

/////////////////////////////////////////////////////////////////////////////////////////

guint gva_tensor_size(GstGVATensorMeta *meta) {
    guint size = 1;
    for (guint i = 0; i < meta->rank && i < GVA_TENSOR_MAX_RANK; i++) {
        if (meta->dims[i])
            size *= meta->dims[i];
    }
    return size;
}
