/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "i_metapublish_method.h"
G_DEFINE_INTERFACE(MetapublishMethod, metapublish_method, G_TYPE_OBJECT)

static void metapublish_method_default_init(MetapublishMethodInterface *iface) {
    (void)iface;
}

gboolean metapublish_method_start(MetapublishMethod *self, GstGvaMetaPublish *gvametapublish) {
    MetapublishMethodInterface *iface;
    g_return_val_if_fail(METAPUBLISH_IS_METHOD(self), FALSE);

    iface = METAPUBLISH_METHOD_GET_IFACE(self);
    g_return_val_if_fail(iface->start != NULL, FALSE);
    return iface->start(self, gvametapublish);
}

gboolean metapublish_method_publish(MetapublishMethod *self, GstGvaMetaPublish *gvametapublish, gchar *json_message) {
    MetapublishMethodInterface *iface;

    g_return_val_if_fail(METAPUBLISH_IS_METHOD(self), FALSE);

    iface = METAPUBLISH_METHOD_GET_IFACE(self);
    g_return_val_if_fail(iface->publish != NULL, FALSE);
    return iface->publish(self, gvametapublish, json_message);
}

gboolean metapublish_method_stop(MetapublishMethod *self, GstGvaMetaPublish *gvametapublish) {
    MetapublishMethodInterface *iface;

    g_return_val_if_fail(METAPUBLISH_IS_METHOD(self), FALSE);

    iface = METAPUBLISH_METHOD_GET_IFACE(self);
    g_return_val_if_fail(iface->stop != NULL, FALSE);
    return iface->stop(self, gvametapublish);
}