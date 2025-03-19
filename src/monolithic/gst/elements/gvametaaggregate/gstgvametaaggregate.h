/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __GST_GVA_META_AGGREGATE_H__
#define __GST_GVA_META_AGGREGATE_H__

#include <gst/base/gstaggregator.h>
#include <gst/video/video.h>

#include <gva_caps.h>

G_BEGIN_DECLS

typedef struct _GstGvaMetaAggregate GstGvaMetaAggregate;
typedef struct _GstGvaMetaAggregateClass GstGvaMetaAggregateClass;
typedef struct _GstGvaMetaAggregatePrivate GstGvaMetaAggregatePrivate;

#define GST_TYPE_GVA_META_AGGREGATE_PAD (gst_gva_meta_aggregate_pad_get_type())
#define GST_GVA_META_AGGREGATE_PAD(obj)                                                                                \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_META_AGGREGATE_PAD, GstGvaMetaAggregatePad))
#define GST_GVA_META_AGGREGATE_PAD_CAST(obj) ((GstGvaMetaAggregatePad *)(obj))
#define GST_GVA_META_AGGREGATE_PAD_CLASS(klass)                                                                        \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_META_AGGREGATE_PAD, GstGvaMetaAggregatePadClass))
#define GST_IS_GVA_META_AGGREGATE_PAD(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_META_AGGREGATE_PAD))
#define GST_IS_GVA_META_AGGREGATE_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_META_AGGREGATE_PAD))
#define GST_GVA_META_AGGREGATE_PAD_GET_CLASS(obj)                                                                      \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_GVA_META_AGGREGATE_PAD, GstGvaMetaAggregatePadClass))

#define GST_GVA_META_AGGREGATE_GET_MUTEX(obj) (&GST_GVA_META_AGGREGATE(obj)->mutex)

#define GST_GVA_META_AGGREGATE_LOCK(obj) g_mutex_lock(GST_GVA_META_AGGREGATE_GET_MUTEX(obj));

#define GST_GVA_META_AGGREGATE_UNLOCK(obj) g_mutex_unlock(GST_GVA_META_AGGREGATE_GET_MUTEX(obj));

typedef struct _GstGvaMetaAggregatePad GstGvaMetaAggregatePad;
typedef struct _GstGvaMetaAggregatePadClass GstGvaMetaAggregatePadClass;

struct _GstGvaMetaAggregatePad {
    GstAggregatorPad parent;
    GstVideoInfo info;
    CapsFeature caps_feature;
    GstBuffer *buffer;
    GstClockTime start_time;
    GstClockTime end_time;
};

struct _GstGvaMetaAggregatePadClass {
    GstAggregatorPadClass parent_class;
};

GType gst_gva_meta_aggregate_pad_get_type(void);

#define GST_TYPE_GVA_META_AGGREGATE (gst_gva_meta_aggregate_get_type())
#define GST_GVA_META_AGGREGATE(obj)                                                                                    \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_META_AGGREGATE, GstGvaMetaAggregate))
#define GST_GVA_META_AGGREGATE_CAST(obj) ((GstGvaMetaAggregate *)(obj))
#define GST_GVA_META_AGGREGATE_CLASS(klass)                                                                            \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_META_AGGREGATE, GstGvaMetaAggregateClass))
#define GST_IS_GVA_META_AGGREGATE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_META_AGGREGATE))
#define GST_IS_GVA_META_AGGREGATE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_META_AGGREGATE))
#define GST_GVA_META_AGGREGATE_GET_CLASS(obj)                                                                          \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_GVA_META_AGGREGATE, GstGvaMetaAggregateClass))

struct _GstGvaMetaAggregate {
    GstAggregator aggregator;
    GstVideoInfo info;
    GMutex mutex;
    GstClockTime ts_offset;
    guint64 nframes;
    GstCaps *current_caps;
};

struct _GstGvaMetaAggregateClass {
    GstAggregatorClass parent_class;
    GstFlowReturn (*aggregate_metas)(GstGvaMetaAggregate *gvametaaggregate, GstBuffer *outbuffer);
};

GType gst_gva_meta_aggregate_get_type(void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstGvaMetaAggregate, gst_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstGvaMetaAggregatePad, gst_object_unref)

G_END_DECLS
#endif /* __GST_GVA_META_AGGREGATE_H__ */
