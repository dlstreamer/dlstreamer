/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __SPLITJOINBIN_H__
#define __SPLITJOINBIN_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_SPLITJOINBIN (splitjoinbin_get_type())
#define GST_SPLITJOINBIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_SPLITJOINBIN, GstSplitJoinBin))
#define GST_SPLITJOINBIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_SPLITJOINBIN, GstSplitJoinBinClass))
#define GST_IS_SPLITJOINBIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_SPLITJOINBIN))
#define GST_IS_SPLITJOINBIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_SPLITJOINBIN))

GType splitjoinbin_get_type(void);

typedef struct _GstSplitJoinBin GstSplitJoinBin;
typedef struct _GstSplitJoinBinClass GstSplitJoinBinClass;

struct _GstSplitJoinBin {
    GstBin bin;

    GstElement *identity;

    GstElement *preprocess;
    GstElement *process;
    GstElement *postprocess;
    GstElement *aggregate;
    GstElement *postaggregate;

    gint preprocess_queue_size;
    gint process_queue_size;
    gint postprocess_queue_size;
    gint aggregate_queue_size;
    gint postaggregate_queue_size;

    GstPad *sink_pad;
    GstPad *src_pad;
};

struct _GstSplitJoinBinClass {
    GstBinClass parent_class;
};

gboolean splitjoinbin_set_elements(GstSplitJoinBin *self, GstElement *preprocess, GstElement *process,
                                   GstElement *postprocess, GstElement *aggregate, GstElement *postaggregate);

gboolean splitjoinbin_set_elements_description(GstSplitJoinBin *self, const gchar *preprocess, const gchar *process,
                                               const gchar *postprocess, const gchar *aggregate,
                                               const gchar *postaggregate);

void splitjoinbin_set_queue_size(GstSplitJoinBin *self, int preprocess_queue_size, int process_queue_size,
                                 int postprocess_queue_size, int aggregate_queue_size, int postaggregate_queue_size);

gboolean splitjoinbin_link_elements(GstSplitJoinBin *self);

gboolean splitjoinbin_is_linked(GstSplitJoinBin *self);

G_END_DECLS

#endif /* __SPLITJOINBIN_H__ */
