/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <string.h>

#include "dlstreamer/gst/metadata/gva_tensor_meta.h"

#define UNUSED(x) (void)(x)

DLS_EXPORT GType gst_gva_tensor_meta_api_get_type(void) {
    static GType type;
    static const gchar *tags[] = {NULL};

    if (g_once_init_enter(&type)) {
        GType _type = gst_meta_api_type_register(GVA_TENSOR_META_API_NAME, tags);
        g_once_init_leave(&type, _type);
    }
    return type;
}

gboolean gst_gva_tensor_meta_init(GstMeta *meta, gpointer params, GstBuffer *buffer) {
    UNUSED(params);
    UNUSED(buffer);

    GstGVATensorMeta *tensor_meta = (GstGVATensorMeta *)meta;
    tensor_meta->data = gst_structure_new_empty("meta");
    return TRUE;
}

void gst_gva_tensor_meta_free(GstMeta *meta, GstBuffer *buffer) {
    UNUSED(buffer);

    GstGVATensorMeta *tensor_meta = (GstGVATensorMeta *)meta;
    if (tensor_meta->data) {
        gst_structure_remove_all_fields(tensor_meta->data);
        gst_structure_free(tensor_meta->data);
        tensor_meta->data = NULL;
    }
}

gboolean gst_gva_tensor_meta_transform(GstBuffer *dest_buf, GstMeta *src_meta, GstBuffer *src_buf, GQuark type,
                                       gpointer data) {
    UNUSED(src_buf);
    UNUSED(type);
    UNUSED(data);

    g_return_val_if_fail(gst_buffer_is_writable(dest_buf), FALSE);

    GstGVATensorMeta *dst = GST_GVA_TENSOR_META_ADD(dest_buf);
    GstGVATensorMeta *src = (GstGVATensorMeta *)src_meta;

    if (dst->data) {
        gst_structure_remove_all_fields(dst->data);
        gst_structure_free(dst->data);
    }
    dst->data = gst_structure_copy(src->data);

    return TRUE;
}

DLS_EXPORT const GstMetaInfo *gst_gva_tensor_meta_get_info(void) {
    static const GstMetaInfo *meta_info = NULL;

    if (g_once_init_enter(&meta_info)) {
        const GstMetaInfo *meta = gst_meta_register(
            gst_gva_tensor_meta_api_get_type(), GVA_TENSOR_META_IMPL_NAME, sizeof(GstGVATensorMeta),
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
            if (!gst_structure_has_field(meta->data, "model_name") ||
                strstr(gst_structure_get_string(meta->data, "model_name"), model_name) == NULL)
                continue;
        }
        if (output_layer) {
            if (!gst_structure_has_field(meta->data, "layer_name") ||
                strstr(gst_structure_get_string(meta->data, "layer_name"), output_layer) == NULL)
                continue;
        }
        if (element_id) {
            if (!gst_structure_has_field(meta->data, "element_id") ||
                strstr(gst_structure_get_string(meta->data, "element_id"), element_id) == NULL)
                continue;
        }
        return meta;
    }

    return NULL;
}

GstGVATensorMeta *find_tensor_meta(GstBuffer *buffer, const char *model_name, const char *output_layer) {
    return find_tensor_meta_ext(buffer, model_name, output_layer, NULL);
}
