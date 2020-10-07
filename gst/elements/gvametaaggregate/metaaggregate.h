/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#ifndef __META_AGGREGATE_H__
#define __META_AGGREGATE_H__

#include "gstgvametaaggregate.h"
#include <gst/base/gstaggregator.h>
#include <gst/gst.h>
#include <gst/gstbuffer.h>

G_BEGIN_DECLS

GstFlowReturn aggregate_metas(GstGvaMetaAggregate *magg, GstBuffer *outbuf);
GstFlowReturn gst_gva_meta_aggregate_fill_queues(GstGvaMetaAggregate *gvametaaggregate,
                                                 GstClockTime output_start_running_time,
                                                 GstClockTime output_end_running_time);
gboolean sync_pad_values(GstElement *gvametaaggregate, GstPad *pad, gpointer user_data);
void gst_gva_meta_aggregate_advance_on_timeout(GstGvaMetaAggregate *gvametaaggregate);
GstFlowReturn gst_gva_meta_aggregate_do_aggregate(GstGvaMetaAggregate *gvametaaggregate, GstClockTime output_start_time,
                                                  GstClockTime output_end_time, GstBuffer **outbuf);
GstFlowReturn gst_gva_meta_aggregate_aggregate(GstAggregator *agg, gboolean timeout);

G_END_DECLS

#endif
