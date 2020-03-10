/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
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
    tensor_meta->data = gst_structure_new_empty("meta");
    return TRUE;
}

void gst_gva_tensor_meta_free(GstMeta *meta, GstBuffer *buffer) {
    UNUSED(buffer);

    GstGVATensorMeta *tensor_meta = (GstGVATensorMeta *)meta;
    gst_structure_remove_all_fields(tensor_meta->data);
    gst_structure_free(tensor_meta->data);
}

const GstMetaInfo *gst_gva_tensor_meta_get_info(void) {
    static const GstMetaInfo *meta_info = NULL;

    if (g_once_init_enter(&meta_info)) {
        const GstMetaInfo *meta =
            gst_meta_register(gst_gva_tensor_meta_api_get_type(), "GstGVATensorMeta", sizeof(GstGVATensorMeta),
                              (GstMetaInitFunction)gst_gva_tensor_meta_init,
                              (GstMetaFreeFunction)gst_gva_tensor_meta_free, (GstMetaTransformFunction)NULL);
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
