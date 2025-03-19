/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __PROCESSBIN_H__
#define __PROCESSBIN_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_PROCESSBIN (processbin_get_type())
#define GST_PROCESSBIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_PROCESSBIN, GstProcessBin))
#define GST_PROCESSBIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_PROCESSBIN, GstProcessBinClass))
#define GST_IS_PROCESSBIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_PROCESSBIN))
#define GST_IS_PROCESSBIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_PROCESSBIN))

GType processbin_get_type(void);

typedef struct _GstProcessBin GstProcessBin;
typedef struct _GstProcessBinClass GstProcessBinClass;

struct _GstProcessBin {
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

struct _GstProcessBinClass {
    GstBinClass parent_class;
};

gboolean processbin_set_elements(GstProcessBin *self, GstElement *preprocess, GstElement *process,
                                 GstElement *postprocess, GstElement *aggregate, GstElement *postaggregate);

gboolean processbin_set_elements_description(GstProcessBin *self, const gchar *preprocess, const gchar *process,
                                             const gchar *postprocess, const gchar *aggregate,
                                             const gchar *postaggregate);

void processbin_set_queue_size(GstProcessBin *self, int preprocess_queue_size, int process_queue_size,
                               int postprocess_queue_size, int aggregate_queue_size, int postaggregate_queue_size);

gboolean processbin_link_elements(GstProcessBin *self);

gboolean processbin_is_linked(GstProcessBin *self);

G_END_DECLS

#endif /* __PROCESSBIN_H__ */
