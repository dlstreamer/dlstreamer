/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __GVA_JSON_INFO_H__
#define __GVA_JSON_INFO_H__

#include <gst/gst.h>

#define GVA_JSON_META_TAG "gva_json_meta"

G_BEGIN_DECLS

typedef struct _GstGVAJSONMeta GstGVAJSONMeta;

struct _GstGVAJSONMeta {
    GstMeta meta;
    gchar *message;
};

const GstMetaInfo *gst_gva_json_meta_get_info(void);
GType gst_gva_json_meta_api_get_type(void);
#define GST_GVA_JSON_META_INFO (gst_gva_json_meta_get_info())
#define GST_GVA_JSON_META_GET(buf) ((GstGVAJSONMeta *)gst_buffer_get_meta(buf, gst_gva_json_meta_api_get_type()))
#define GST_GVA_JSON_META_ITERATE(buf, state)                                                                          \
    ((GstGVAJSONMeta *)gst_buffer_iterate_meta_filtered(buf, state, gst_gva_json_meta_api_get_type()))
#define GST_GVA_JSON_META_ADD(buf) ((GstGVAJSONMeta *)gst_buffer_add_meta(buf, gst_gva_json_meta_get_info(), NULL))

gchar *get_json_message(GstGVAJSONMeta *meta);

G_END_DECLS

#endif /* __GVA_JSON_INFO_H__ */
