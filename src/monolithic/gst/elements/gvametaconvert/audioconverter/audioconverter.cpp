/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "audioconverter.h"
#include "audio_frame.h"
#include "convert_tensor.h"
#include "gva_utils.h"
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

json convert_event_detection(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    assert(converter && buffer && "Expected valid pointers GstGvaMetaConvert and GstBuffer");

    json res;
    GVA::AudioFrame audio_frame(buffer, converter->audio_info);
    std::vector<GVA::AudioEvent> events = audio_frame.events();
    for (std::vector<GVA::AudioEvent>::iterator it = events.begin(); it != events.end(); ++it) {

        json jobject = json::object();

        if (converter->add_tensor_data) {
            jobject["tensors"] = json::array();
        }

        jobject.push_back({"start_timestamp", it->_meta()->start_timestamp});
        jobject.push_back({"end_timestamp", it->_meta()->end_timestamp});

        const gchar *event_type = g_quark_to_string(it->_meta()->event_type);

        if (event_type) {
            jobject.push_back({"event_type", event_type});
        }

        for (GList *l = it->_meta()->params; l; l = g_list_next(l)) {

            GstStructure *s = GST_STRUCTURE(l->data);
            const gchar *s_name = gst_structure_get_name(s);
            if (strcmp(s_name, "detection") == 0) {

                guint64 start_timestamp;
                guint64 end_timestamp;
                if (gst_structure_get(s, "start_timestamp", G_TYPE_UINT64, &start_timestamp, "end_timestamp",
                                      G_TYPE_UINT64, &end_timestamp, NULL)) {
                    json detection = {
                        {"segment", {{"start_timestamp", start_timestamp}, {"end_timestamp", end_timestamp}}}};

                    double confidence;
                    if (gst_structure_get(s, "confidence", G_TYPE_DOUBLE, &confidence, NULL)) {
                        confidence = static_cast<int>(confidence * 100) / static_cast<double>(100);
                        detection.push_back({"confidence", confidence});
                    }
                    int label_id;
                    if (gst_structure_get(s, "label_id", G_TYPE_INT, &label_id, NULL)) {
                        detection.push_back({"label_id", label_id});
                    }

                    const gchar *event = g_quark_to_string(it->_meta()->event_type);

                    if (event) {
                        detection.push_back({"label", event});
                    }
                    jobject.push_back(json::object_t::value_type("detection", detection));
                }
            } else {
                char *label;
                char *model_name;
                double confidence;
                if (gst_structure_get(s, "label", G_TYPE_STRING, &label, "model_name", G_TYPE_STRING, &model_name,
                                      "confidence", G_TYPE_DOUBLE, &confidence, NULL)) {
                    confidence = static_cast<int>(confidence * 100) / static_cast<double>(100);
                    const gchar *attribute_name = gst_structure_get_string(s, "attribute_name")
                                                      ? gst_structure_get_string(s, "attribute_name")
                                                      : s_name;
                    jobject.push_back(json::object_t::value_type(
                        attribute_name,
                        {{"label", label}, {"confidence", confidence}, {"model", {{"name", model_name}}}}));
                    g_free(label);
                    g_free(model_name);
                }
            }
            if (converter->add_tensor_data) {
                GVA::Tensor s_tensor = GVA::Tensor((GstStructure *)l->data);
                jobject["tensors"].push_back(convert_tensor(s_tensor));
            }
        }
        if (!jobject.empty()) {
            if (res["events"].is_null()) {
                res["events"] = json::array();
            }
            res["events"].push_back(jobject);
        }
    }
    return res;
}

json get_audio_frame_data(GstGvaMetaConvert *converter) {
    assert(converter && "Expected valid pointer GstGvaMetaConvert");

    json res;
    if (converter->audio_info) {
        res["rate"] = converter->audio_info->rate;
        res["channels"] = converter->audio_info->channels;
    }
    if (converter->source)
        res["source"] = converter->source;
    if (converter->tags && json::accept(converter->tags))
        res["tags"] = json::parse(converter->tags);

    return res;
}

} // namespace

void dump_audio_detection(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    if (!converter)
        throw std::invalid_argument("GvaMetaConvert is null");
    if (!buffer)
        throw std::invalid_argument("GstBuffer is null");

    GVA::AudioFrame audio_frame(buffer, converter->audio_info);
    for (GVA::AudioEvent &event : audio_frame.events()) {
        auto segment = event.segment();
        GST_INFO_OBJECT(converter,
                        "Detection: "
                        "start_timestamp: %lu, end_timestamp: %lu, event_type: %s",
                        segment.start, segment.end, event.label().c_str());
    }
}

gboolean convert_audio_meta_to_json(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    if (!converter)
        throw std::invalid_argument("GvaMetaConvert is null");
    if (!buffer)
        throw std::invalid_argument("GstBuffer is null");

    json jframe = get_audio_frame_data(converter);
    json jevent_detection = convert_event_detection(converter, buffer);
    if (jevent_detection.empty()) {
        if (!converter->add_empty_detection_results) {
            GST_DEBUG_OBJECT(converter, "No detections found. Not posting JSON message");
            return TRUE;
        }
    }
    if (!jframe.is_null()) {
        if (!jevent_detection.empty()) {
            jframe.update(jevent_detection);
        }
        std::string json_message = jframe.dump(converter->json_indent);
        GVA::AudioFrame audio_frame(buffer, converter->audio_info);
        audio_frame.add_message(json_message);
        GST_INFO_OBJECT(converter, "JSON message: %s", json_message.c_str());
    }
    return TRUE;
}
