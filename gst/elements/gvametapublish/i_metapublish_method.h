/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef __METAPUBLISH_METHOD_H__
#define __METAPUBLISH_METHOD_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define METAPUBLISH_TYPE_METHOD metapublish_method_get_type()
G_DECLARE_INTERFACE(MetapublishMethod, metapublish_method, METAPUBLISH, METHOD, GObject)

#include "gstgvametapublish.h"

struct _MetapublishMethodInterface {
    GTypeInterface parent_iface;

    gboolean (*start)(MetapublishMethod *self, GstGvaMetaPublish *gvametapublish);
    gboolean (*publish)(MetapublishMethod *self, GstGvaMetaPublish *gvametapublish, gchar *json_message);
    gboolean (*stop)(MetapublishMethod *self, GstGvaMetaPublish *gvametapublish);
};

gboolean metapublish_method_start(MetapublishMethod *self, GstGvaMetaPublish *gvametapublish);
gboolean metapublish_method_publish(MetapublishMethod *self, GstGvaMetaPublish *gvametapublish, gchar *json_message);
gboolean metapublish_method_stop(MetapublishMethod *self, GstGvaMetaPublish *gvametapublish);

G_END_DECLS

#endif /* __METAPUBLISH_METHOD_H__ */