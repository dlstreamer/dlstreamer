/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
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

typedef void (*broker_function_type)(GstGvaMetaPublish *method, GstBuffer *buffer);
typedef gboolean (*broker_initfunction_type)(GstGvaMetaPublish *method);
typedef void (*broker_finalizefunction_type)(GstGvaMetaPublish *method);

struct _GstGvaMetaPublish {
    GstBaseTransform base_gvametapublish;
    gchar *method;
    gchar *file_path;
    gchar *output_format;
    gchar *host;
    gchar *address;
    gchar *clientid;
    gchar *topic;
    gchar *timeout;
    broker_initfunction_type broker_initializefunction;
    broker_function_type broker_function;
    broker_finalizefunction_type broker_finalizefunction;
    gboolean signal_handoffs;
};

struct _GstGvaMetaPublishClass {
    GstBaseTransformClass base_gvametapublish_class;
    void (*handoff)(GstElement *element, GstBuffer *buf);
};

GType gst_gva_meta_publish_get_type(void);

G_END_DECLS

#endif
