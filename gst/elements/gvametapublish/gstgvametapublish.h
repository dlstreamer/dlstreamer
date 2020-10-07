/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef _GST_GVA_META_PUBLISH_H_
#define _GST_GVA_META_PUBLISH_H_

#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_GVA_META_PUBLISH (gst_gva_meta_publish_get_type())
#define GST_GVA_META_PUBLISH(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_META_PUBLISH, GstGvaMetaPublish))
#define GST_GVA_META_PUBLISH_CLASS(klass)                                                                              \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_META_PUBLISH, GstGvaMetaPublishClass))
#define GST_IS_GVA_META_PUBLISH(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_META_PUBLISH))
#define GST_IS_GVA_META_PUBLISH_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_META_PUBLISH))

typedef struct _GstGvaMetaPublish GstGvaMetaPublish;
typedef struct _GstGvaMetaPublishClass GstGvaMetaPublishClass;

#include "i_metapublish_method.h"

typedef enum {
    GST_GVA_METAPUBLISH_FILE = 1,
#ifdef PAHO_INC
    GST_GVA_METAPUBLISH_MQTT = 2,
#endif
#ifdef KAFKA_INC
    GST_GVA_METAPUBLISH_KAFKA = 3,
#endif
    GST_GVA_METAPUBLISH_NONE = 4
} GstGVAMetaPublishMethodType;

typedef enum { GST_GVA_METAPUBLISH_JSON = 1, GST_GVA_METAPUBLISH_JSON_LINES = 2 } GstGVAMetaPublishFileFormat;

struct _GstGvaMetaPublish {
    GstBaseTransform base_gvametapublish;
    GstGVAMetaPublishMethodType method;
    gchar *file_path;
    GstGVAMetaPublishFileFormat file_format;
    gchar *address;
    gchar *mqtt_client_id;
    gchar *topic;
    gchar *timeout;
    guint max_connect_attempts;
    guint max_reconnect_interval;
    gboolean signal_handoffs;
    MetapublishMethod *method_class;
};

struct _GstGvaMetaPublishClass {
    GstBaseTransformClass base_gvametapublish_class;
    void (*handoff)(GstElement *element, GstBuffer *buf);
};

GType gst_gva_meta_publish_get_type(void);

G_END_DECLS

#endif
