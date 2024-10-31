/*******************************************************************************
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/analytics/analytics-meta-prelude.h>
#include <gst/analytics/gstanalyticsmeta.h>
#include <gst/gst.h>

inline const bool NEW_METADATA = false;

typedef struct _GstAnalyticsMtd GstAnalyticsODExtMtd;

typedef struct _GstAnalyticsODExtMtdData GstAnalyticsODExtMtdData;

struct _GstAnalyticsODExtMtdData {
    gint class_id;
    gdouble rotation;
    GList *params;
};

inline const GstAnalyticsMtdImpl od_ext_impl = {"object-detection-extended", NULL, {NULL}};

inline GstAnalyticsMtdType gst_analytics_od_ext_mtd_get_mtd_type(void) {
    return (GstAnalyticsMtdType)&od_ext_impl;
}

inline gboolean gst_analytics_od_ext_mtd_get_rotation(const GstAnalyticsODExtMtd *handle, gdouble *rotation) {
    GstAnalyticsODExtMtdData *data;

    g_return_val_if_fail(handle && rotation, false);
    data = (GstAnalyticsODExtMtdData *)gst_analytics_relation_meta_get_mtd_data(handle->meta, handle->id);
    g_return_val_if_fail(data, false);

    *rotation = data->rotation;

    return TRUE;
}

inline gboolean gst_analytics_od_ext_mtd_get_class_id(const GstAnalyticsODExtMtd *handle, gint *class_id) {
    GstAnalyticsODExtMtdData *data;

    g_return_val_if_fail(handle && class_id, false);
    data = (GstAnalyticsODExtMtdData *)gst_analytics_relation_meta_get_mtd_data(handle->meta, handle->id);
    g_return_val_if_fail(data, false);

    *class_id = data->class_id;

    return TRUE;
}

inline GList *gst_analytics_od_ext_mtd_get_params(const GstAnalyticsODExtMtd *handle) {
    GstAnalyticsODExtMtdData *data;
    g_return_val_if_fail(handle, nullptr);
    data = (GstAnalyticsODExtMtdData *)gst_analytics_relation_meta_get_mtd_data(handle->meta, handle->id);
    g_return_val_if_fail(data, nullptr);
    return data->params;
}

inline gboolean gst_analytics_od_ext_mtd_add_param(const GstAnalyticsODExtMtd *handle, GstStructure *s) {
    g_return_val_if_fail(handle, false);
    g_return_val_if_fail(s, false);

    GstAnalyticsODExtMtdData *data;
    data = (GstAnalyticsODExtMtdData *)gst_analytics_relation_meta_get_mtd_data(handle->meta, handle->id);
    if (data) {
        data->params = g_list_append(data->params, s);
    }
    return data != nullptr;
}

inline GstStructure *gst_analytics_od_ext_mtd_get_param(const GstAnalyticsODExtMtd *handle, const gchar *name) {
    g_return_val_if_fail(handle, nullptr);
    g_return_val_if_fail(name, nullptr);

    GList *l;
    GstAnalyticsODExtMtdData *data;
    data = (GstAnalyticsODExtMtdData *)gst_analytics_relation_meta_get_mtd_data(handle->meta, handle->id);
    g_return_val_if_fail(data, nullptr);

    for (l = data->params; l; l = g_list_next(l)) {
        GstStructure *s = (GstStructure *)l->data;

        if (gst_structure_has_name(s, name))
            return s;
    }

    return nullptr;
}

inline gboolean gst_analytics_relation_meta_add_od_ext_mtd(GstAnalyticsRelationMeta *instance, gdouble rotation,
                                                           gint class_id, GstAnalyticsODExtMtd *od_ext_mtd) {
    g_return_val_if_fail(instance, false);
    gsize size = sizeof(GstAnalyticsODExtMtdData);
    GstAnalyticsODExtMtdData *od_ext_mtd_data =
        (GstAnalyticsODExtMtdData *)gst_analytics_relation_meta_add_mtd(instance, &od_ext_impl, size, od_ext_mtd);
    if (od_ext_mtd_data) {
        od_ext_mtd_data->rotation = rotation;
        od_ext_mtd_data->class_id = class_id;
        od_ext_mtd_data->params = nullptr;
    }
    return od_ext_mtd_data != nullptr;
}

inline gboolean gst_analytics_relation_meta_get_od_ext_mtd(GstAnalyticsRelationMeta *meta, gint an_meta_id,
                                                           GstAnalyticsODExtMtd *rlt) {
    return gst_analytics_relation_meta_get_mtd(meta, an_meta_id, gst_analytics_od_ext_mtd_get_mtd_type(),
                                               (GstAnalyticsODExtMtd *)rlt);
}
