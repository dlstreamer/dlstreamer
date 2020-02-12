/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <string.h>

#include "gva_json_meta.h"

#define UNUSED(x) (void)(x)

GType gst_gva_json_meta_api_get_type(void) {
    static volatile GType type;
    static const gchar *tags[] = {GVA_JSON_META_TAG, NULL};

    if (g_once_init_enter(&type)) {
        GType _type = gst_meta_api_type_register("GstGVAJSONMetaAPI", tags);
        g_once_init_leave(&type, _type);
    }
    return type;
}

gboolean gst_gva_json_meta_init(GstMeta *meta, gpointer params, GstBuffer *buffer) {
    UNUSED(params);
    UNUSED(buffer);

    GstGVAJSONMeta *json_meta = (GstGVAJSONMeta *)meta;
    json_meta->message = 0;
    return TRUE;
}

gboolean gst_gva_json_meta_transform(GstBuffer *dest_buf, GstMeta *src_meta, GstBuffer *src_buf, GQuark type,
                                     gpointer data) {
    UNUSED(src_buf);
    UNUSED(type);
    UNUSED(data);

    GstGVAJSONMeta *dst = GST_GVA_JSON_META_ADD(dest_buf);
    GstGVAJSONMeta *src = (GstGVAJSONMeta *)src_meta;

    dst->message = g_strdup(src->message);
    return TRUE;
}

void gst_gva_json_meta_free(GstMeta *meta, GstBuffer *buffer) {
    UNUSED(buffer);

    GstGVAJSONMeta *json_meta = (GstGVAJSONMeta *)meta;
    if (json_meta->message) {
        g_free(json_meta->message);
        json_meta->message = NULL;
    }
}

const GstMetaInfo *gst_gva_json_meta_get_info(void) {
    static const GstMetaInfo *meta_info = NULL;

    if (g_once_init_enter(&meta_info)) {
        const GstMetaInfo *meta =
            gst_meta_register(gst_gva_json_meta_api_get_type(), "GstGVAJSONMeta", sizeof(GstGVAJSONMeta),
                              (GstMetaInitFunction)gst_gva_json_meta_init, (GstMetaFreeFunction)gst_gva_json_meta_free,
                              (GstMetaTransformFunction)gst_gva_json_meta_transform);
        g_once_init_leave(&meta_info, meta);
    }
    return meta_info;
}

gchar *get_json_message(GstGVAJSONMeta *meta) {
    return meta->message;
}

void set_json_message(GstGVAJSONMeta *meta, gchar *message) {
    gst_gva_json_meta_free((GstMeta *)meta, NULL);
    meta->message = g_strdup(message);
}
