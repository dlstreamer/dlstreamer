/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>

#include <gst/base/gstaggregator.h>

G_BEGIN_DECLS

#define GST_TYPE_META_AGGREGATE (meta_aggregate_get_type())
#define GST_META_AGGREGATE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_META_AGGREGATE, MetaAggregate))
#define GST_META_AGGREGATE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_META_AGGREGATE, MetaAggregateClass))
#define GST_IS_META_AGGREGATE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_META_AGGREGATE))
#define GST_IS_META_AGGREGATE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_META_AGGREGATE))

GType meta_aggregate_pad_get_type(void);

#define GST_TYPE_META_AGGREGATE_PAD (meta_aggregate_pad_get_type())
#define GST_META_AGGREGATE_PAD(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_META_AGGREGATE_PAD, MetaAggregatePad))
#define GST_META_AGGREGATE_PAD_CLASS(klass)                                                                            \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_META_AGGREGATE_PAD, MetaAggregatePadClass))
#define GST_IS_META_AGGREGATE_PAD(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_META_AGGREGATE_PAD))
#define GST_IS_META_AGGREGATE_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_META_AGGREGATE_PAD))

struct MetaAggregatePad {
    GstAggregatorPad parent;
    class MetaAggregatePadPrivate *impl;
};

struct MetaAggregatePadClass {
    GstAggregatorPadClass parent;
};

struct MetaAggregate {
    GstAggregator parent;
    class MetaAggregatePrivate *impl;
};

struct MetaAggregateClass {
    GstAggregatorClass parent;
};

GType meta_aggregate_get_type(void);

G_END_DECLS
