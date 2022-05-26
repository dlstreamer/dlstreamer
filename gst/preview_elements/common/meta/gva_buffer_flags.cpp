/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gva_buffer_flags.hpp"
#include "tensor_layer_desc.hpp"

GstQuery *gva_query_new_model_input() {
    auto structure = gst_structure_new_empty("model_input");
    return gst_query_new_custom(static_cast<GstQueryType>(GVA_QUERY_MODEL_INPUT), structure);
}

bool gva_query_parse_model_input(GstQuery *query, TensorLayerDesc &model_input) {
    g_return_val_if_fail(GST_IS_QUERY(query), false);
    g_return_val_if_fail(GST_QUERY_TYPE(query) == static_cast<GstQueryType>(GVA_QUERY_MODEL_INPUT), false);

    auto structure = gst_query_get_structure(query);
    GArray *inputs_array = nullptr;
    if (!gst_structure_get(structure, "inputs", G_TYPE_ARRAY, &inputs_array, nullptr) || !inputs_array)
        return false;

    assert(inputs_array->len == 1);
    model_input = g_array_index(inputs_array, TensorLayerDesc, 0);
    g_array_unref(inputs_array);

    return true;
}

bool gva_query_fill_model_input(GstQuery *query, const TensorLayerDesc &model_input) {
    g_return_val_if_fail(GST_IS_QUERY(query), false);
    g_return_val_if_fail(GST_QUERY_TYPE(query) == static_cast<GstQueryType>(GVA_QUERY_MODEL_INPUT), false);

    auto structure = gst_query_writable_structure(query);
    auto inputs_array = g_array_new(false, false, sizeof(TensorLayerDesc));
    g_array_append_val(inputs_array, model_input);
    gst_structure_set(structure, "inputs", G_TYPE_ARRAY, inputs_array, nullptr);
    g_array_unref(inputs_array);

    return true;
}

GstQuery *gva_query_new_model_output() {
    auto structure = gst_structure_new_empty("model_output");
    return gst_query_new_custom(static_cast<GstQueryType>(GVA_QUERY_MODEL_OUTPUT), structure);
}

bool gva_query_parse_model_output(GstQuery *query, std::vector<TensorLayerDesc> &model_output) {
    g_return_val_if_fail(GST_IS_QUERY(query), false);
    g_return_val_if_fail(GST_QUERY_TYPE(query) == static_cast<GstQueryType>(GVA_QUERY_MODEL_OUTPUT), false);

    GArray *output_array = nullptr;
    auto structure = gst_query_get_structure(query);
    if (!gst_structure_get(structure, "outputs", G_TYPE_ARRAY, &output_array, nullptr) || !output_array)
        return false;

    model_output.reserve(output_array->len);
    for (auto i = 0u; i < output_array->len; i++)
        model_output.push_back(g_array_index(output_array, TensorLayerDesc, i));
    g_array_unref(output_array);

    return true;
}

bool gva_query_fill_model_output(GstQuery *query, const std::vector<TensorLayerDesc> &model_output) {
    g_return_val_if_fail(GST_IS_QUERY(query), false);
    g_return_val_if_fail(GST_QUERY_TYPE(query) == static_cast<GstQueryType>(GVA_QUERY_MODEL_OUTPUT), false);

    auto structure = gst_query_writable_structure(query);
    auto outputs_array = g_array_new(false, false, sizeof(TensorLayerDesc));
    for (const auto &desc : model_output)
        g_array_append_val(outputs_array, desc);
    gst_structure_set(structure, "outputs", G_TYPE_ARRAY, outputs_array, nullptr);
    g_array_unref(outputs_array);

    return true;
}

GstQuery *gva_query_new_model_info() {
    auto structure = gst_structure_new_empty("model_info");
    return gst_query_new_custom(static_cast<GstQueryType>(GVA_QUERY_MODEL_INFO), structure);
}

bool gva_query_parse_model_info(GstQuery *query, std::string &model_name, std::string &instance_id) {
    g_return_val_if_fail(GST_IS_QUERY(query), false);
    g_return_val_if_fail(GST_QUERY_TYPE(query) == static_cast<GstQueryType>(GVA_QUERY_MODEL_INFO), false);

    auto structure = gst_query_get_structure(query);
    auto name = gst_structure_get_string(structure, "model_name");
    auto id = gst_structure_get_string(structure, "instance_id");
    if (!name || !id)
        return false;

    model_name = name;
    instance_id = id;

    return true;
}

bool gva_query_fill_model_info(GstQuery *query, const std::string &model_name, const std::string &instance_id) {
    g_return_val_if_fail(GST_IS_QUERY(query), false);
    g_return_val_if_fail(GST_QUERY_TYPE(query) == static_cast<GstQueryType>(GVA_QUERY_MODEL_INFO), false);

    auto structure = gst_query_writable_structure(query);
    gst_structure_set(structure, "model_name", G_TYPE_STRING, model_name.c_str(), "instance_id", G_TYPE_STRING,
                      instance_id.c_str(), nullptr);

    return true;
}

GstQuery *gva_query_new_postproc_srcpad() {
    auto structure = gst_structure_new_empty("postproc_srcpad");
    return gst_query_new_custom(static_cast<GstQueryType>(GVA_QUERY_POSTPROC_SRCPAD_INFO), structure);
}

bool gva_query_parse_postproc_srcpad(GstQuery *query, GstPad *&postproc_srcpad) {
    g_return_val_if_fail(GST_IS_QUERY(query), false);
    g_return_val_if_fail(GST_QUERY_TYPE(query) == static_cast<GstQueryType>(GVA_QUERY_POSTPROC_SRCPAD_INFO), false);

    auto structure = gst_query_get_structure(query);
    return gst_structure_get(structure, "srcpad", GST_TYPE_PAD, &postproc_srcpad, nullptr) && postproc_srcpad;
}

bool gva_query_fill_postproc_srcpad(GstQuery *query, GstPad *postproc_srcpad) {
    g_return_val_if_fail(GST_IS_QUERY(query), false);
    g_return_val_if_fail(GST_QUERY_TYPE(query) == static_cast<GstQueryType>(GVA_QUERY_POSTPROC_SRCPAD_INFO), false);

    auto structure = gst_query_writable_structure(query);
    gst_structure_set(structure, "srcpad", GST_TYPE_PAD, postproc_srcpad, nullptr);

    return true;
}

GstEvent *gva_event_new_preproc_info(GstVideoInfo *video_info, int32_t resize_algorithm, uint32_t color_format,
                                     gpointer va_display) {
    auto data = gst_structure_new("pre-proc-info", "video-info", GST_TYPE_VIDEO_INFO, video_info, "resize-algo",
                                  G_TYPE_INT, resize_algorithm, "color-format", G_TYPE_UINT, color_format, "va-display",
                                  G_TYPE_POINTER, va_display, nullptr);
    return gst_event_new_custom(static_cast<GstEventType>(GvaEventTypes::GVA_EVENT_PREPROC_INFO), data);
}

bool gva_event_parse_preproc_info(GstEvent *event, GstVideoInfo *&video_info, int32_t &resize_algorithm,
                                  uint32_t &color_format, gpointer &va_display) {
    g_return_val_if_fail(GST_IS_EVENT(event), false);
    g_return_val_if_fail(GST_EVENT_TYPE(event) == static_cast<GstEventType>(GVA_EVENT_PREPROC_INFO), false);

    auto structure = gst_event_get_structure(event);
    return gst_structure_get(structure, "video-info", GST_TYPE_VIDEO_INFO, &video_info, "resize-algo", G_TYPE_INT,
                             &resize_algorithm, "color-format", G_TYPE_UINT, &color_format, "va-display",
                             G_TYPE_POINTER, &va_display, nullptr);
}

GstEvent *gva_event_new_gap_with_buffer(GstBuffer *buffer) {
    auto event = gst_event_new_gap(GST_BUFFER_PTS(buffer), GST_BUFFER_DURATION(buffer));
    auto structure = gst_event_writable_structure(event);
    gst_structure_set(structure, "buffer", GST_TYPE_BUFFER, buffer, nullptr);
    return event;
}

bool gva_event_parse_gap_with_buffer(GstEvent *event, GstBuffer **buffer) {
    g_return_val_if_fail(GST_IS_EVENT(event), false);
    g_return_val_if_fail(GST_EVENT_TYPE(event) == GST_EVENT_GAP, false);

    auto structure = gst_event_get_structure(event);
    return gst_structure_get(structure, "buffer", GST_TYPE_BUFFER, buffer, nullptr);
}
