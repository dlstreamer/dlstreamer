/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>

#include <gst/base/gstaggregator.h>

G_BEGIN_DECLS

GST_DEBUG_CATEGORY_EXTERN(tensormux_debug);
#define GST_DEBUG_CAT_TENSORMUX tensormux_debug

#define GST_TYPE_TENSORMUX (tensormux_get_type())
#define GST_TENSORMUX(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_TENSORMUX, TensorMux))
#define GST_TENSORMUX_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_TENSORMUX, TensorMuxClass))
#define GST_IS_TENSORMUX(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_TENSORMUX))
#define GST_IS_TENSORMUX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_TENSORMUX))

GType tensormux_pad_get_type(void);

#define GST_TYPE_TENSORMUX_PAD (tensormux_pad_get_type())
#define GST_TENSORMUX_PAD(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_TENSORMUX_PAD, TensorMuxPad))
#define GST_TENSORMUX_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_TENSORMUX_PAD, TensorMuxPadClass))
#define GST_IS_TENSORMUX_PAD(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_TENSORMUX_PAD))
#define GST_IS_TENSORMUX_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_TENSORMUX_PAD))

struct TensorMuxPad {
    GstAggregatorPad parent;
    // TODO: add some useful fields
};

struct TensorMuxPadClass {
    GstAggregatorPadClass parent;
};

struct TensorMux {
    GstAggregator parent;
    class TensorMuxPrivate *impl;
};

struct TensorMuxClass {
    GstAggregatorClass parent;
};

GType tensormux_get_type(void);

G_END_DECLS
