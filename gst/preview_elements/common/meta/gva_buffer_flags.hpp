/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>
#include <gst/video/gstvideometa.h>
#include <string>
#include <vector>

enum GvaBufferFlags {
    GVA_BUFFER_FLAG_LAST_ROI_ON_FRAME = (GST_BUFFER_FLAG_LAST << 1),
    GST_BUFFER_FLAG_READY_TO_PUSH = (GST_BUFFER_FLAG_LAST << 2)
};

enum GvaQueryTypes {
    GVA_QUERY_MODEL_INPUT = GST_QUERY_MAKE_TYPE(500, GST_QUERY_TYPE_BOTH),
    GVA_QUERY_MODEL_OUTPUT = GST_QUERY_MAKE_TYPE(501, GST_QUERY_TYPE_UPSTREAM),
    GVA_QUERY_MODEL_INFO = GST_QUERY_MAKE_TYPE(502, GST_QUERY_TYPE_BOTH),
    GVA_QUERY_POSTPROC_SRCPAD_INFO = GST_QUERY_MAKE_TYPE(504, GST_QUERY_TYPE_DOWNSTREAM)
};

enum GvaEventTypes {
    GVA_EVENT_PREPROC_INFO = GST_EVENT_MAKE_TYPE(700, GST_EVENT_TYPE_STICKY | GST_EVENT_TYPE_DOWNSTREAM)
};

struct TensorLayerDesc;

GstQuery *gva_query_new_model_input();
bool gva_query_parse_model_input(GstQuery *query, TensorLayerDesc &model_input);
bool gva_query_fill_model_input(GstQuery *query, const TensorLayerDesc &model_input);

GstQuery *gva_query_new_model_output();
bool gva_query_parse_model_output(GstQuery *query, std::vector<TensorLayerDesc> &model_output);
bool gva_query_fill_model_output(GstQuery *query, const std::vector<TensorLayerDesc> &model_output);

GstQuery *gva_query_new_model_info();
bool gva_query_parse_model_info(GstQuery *query, std::string &model_name, std::string &instance_id);
bool gva_query_fill_model_info(GstQuery *query, const std::string &model_name, const std::string &instance_id);

GstQuery *gva_query_new_postproc_srcpad();
bool gva_query_parse_postproc_srcpad(GstQuery *query, GstPad *&postproc_srcpad);
bool gva_query_fill_postproc_srcpad(GstQuery *query, GstPad *postproc_srcpad);

GstEvent *gva_event_new_preproc_info(GstVideoInfo *video_info, int32_t resize_algorithm, uint32_t color_format,
                                     gpointer va_display);
bool gva_event_parse_preproc_info(GstEvent *event, GstVideoInfo *&video_info, int32_t &resize_algorithm,
                                  uint32_t &color_format, gpointer &va_display);

GstEvent *gva_event_new_gap_with_buffer(GstBuffer *buffer);
bool gva_event_parse_gap_with_buffer(GstEvent *event, GstBuffer **buffer);
