/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __GST_ANALYTICS_OD_EXT_MTD__
#define __GST_ANALYTICS_OD_EXT_MTD__

#include <gst/analytics/analytics-meta-prelude.h>
#include <gst/analytics/gstanalyticsmeta.h>
#include <gst/gst.h>

const int NEW_METADATA = 0;

G_BEGIN_DECLS

typedef struct _GstAnalyticsMtd GstAnalyticsODExtMtd;

GST_ANALYTICS_META_API
GstAnalyticsMtdType gst_analytics_od_ext_mtd_get_mtd_type(void);

GST_ANALYTICS_META_API
gboolean gst_analytics_od_ext_mtd_get_rotation(const GstAnalyticsODExtMtd *handle, gdouble *rotation);

GST_ANALYTICS_META_API
gboolean gst_analytics_od_ext_mtd_get_class_id(const GstAnalyticsODExtMtd *handle, gint *class_id);

GST_ANALYTICS_META_API
GList *gst_analytics_od_ext_mtd_get_params(const GstAnalyticsODExtMtd *handle);

GST_ANALYTICS_META_API
gboolean gst_analytics_od_ext_mtd_add_param(const GstAnalyticsODExtMtd *handle, GstStructure *s);

GST_ANALYTICS_META_API
GstStructure *gst_analytics_od_ext_mtd_get_param(const GstAnalyticsODExtMtd *handle, const gchar *name);

GST_ANALYTICS_META_API
gboolean gst_analytics_relation_meta_add_od_ext_mtd(GstAnalyticsRelationMeta *instance, gdouble rotation, gint class_id,
                                                    GstAnalyticsODExtMtd *od_ext_mtd);

GST_ANALYTICS_META_API
gboolean gst_analytics_relation_meta_get_od_ext_mtd(GstAnalyticsRelationMeta *meta, gint an_meta_id,
                                                    GstAnalyticsODExtMtd *rlt);

G_END_DECLS
#endif // __gst_analytics_od_ext_MTD__