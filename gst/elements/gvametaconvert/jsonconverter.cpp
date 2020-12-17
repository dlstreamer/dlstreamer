/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "jsonconverter.h"
#include "gva_utils.h"
#include "video_frame.h"
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>

#ifdef AUDIO
#include "audioconverter.h"
#endif
#include "convert_tensor.h"

using json = nlohmann::json;

GST_DEBUG_CATEGORY_STATIC(gst_json_converter_debug);
#define GST_CAT_DEFAULT gst_json_converter_debug

/**
 * @return JSON object which contains parameters such as resolution, timestamp, source and tags.
 */
json get_frame_data(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    json res = json::object();
    GstSegment converter_segment = converter->base_gvametaconvert.segment;
    GstClockTime timestamp = gst_segment_to_stream_time(&converter_segment, GST_FORMAT_TIME, buffer->pts);
    if (converter->info)
        res["resolution"] = json::object({{"width", converter->info->width}, {"height", converter->info->height}});
    if (converter->source)
        res["source"] = converter->source;
    if (timestamp != G_MAXUINT64)
        res["timestamp"] = timestamp - converter_segment.time;
    if (converter->tags && json::accept(converter->tags))
        res["tags"] = json::parse(converter->tags);
    return res;
}

/**
 * @return JSON array which contains ROIs attributes and their detection results.
 * Also contains ROIs classification results if any.
 */
json convert_roi_detection(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    json res = json::array();
    GVA::VideoFrame video_frame(buffer, converter->info);
    for (GVA::RegionOfInterest &roi : video_frame.regions()) {
        gint id = 0;
        get_object_id(roi._meta(), &id);

        json jobject = json::object();

        if (converter->add_tensor_data) {
            jobject["tensors"] = json::array();
        }

        jobject.push_back({"x", roi._meta()->x});
        jobject.push_back({"y", roi._meta()->y});
        jobject.push_back({"w", roi._meta()->w});
        jobject.push_back({"h", roi._meta()->h});

        if (id != 0)
            jobject.push_back({"id", id});

        const gchar *roi_type = g_quark_to_string(roi._meta()->roi_type);

        if (roi_type) {
            jobject.push_back({"roi_type", roi_type});
        }
        for (GList *l = roi._meta()->params; l; l = g_list_next(l)) {

            GstStructure *s = (GstStructure *)l->data;
            const gchar *s_name = gst_structure_get_name(s);
            if (strcmp(s_name, "detection") == 0) {
                double xminval;
                double xmaxval;
                double yminval;
                double ymaxval;
                double confidence;
                int label_id;
                if (gst_structure_get(s, "x_min", G_TYPE_DOUBLE, &xminval, "x_max", G_TYPE_DOUBLE, &xmaxval, "y_min",
                                      G_TYPE_DOUBLE, &yminval, "y_max", G_TYPE_DOUBLE, &ymaxval, NULL)) {
                    json detection = {
                        {"bounding_box",
                         {{"x_min", xminval}, {"x_max", xmaxval}, {"y_min", yminval}, {"y_max", ymaxval}}}};

                    if (gst_structure_get(s, "confidence", G_TYPE_DOUBLE, &confidence, NULL)) {
                        detection.push_back({"confidence", confidence});
                    }

                    if (gst_structure_get(s, "label_id", G_TYPE_INT, &label_id, NULL)) {
                        detection.push_back({"label_id", label_id});
                    }

                    const gchar *label = g_quark_to_string(roi._meta()->roi_type);

                    if (label) {
                        detection.push_back({"label", label});
                    }
                    jobject.push_back(json::object_t::value_type("detection", detection));
                }
            } else {
                char *label;
                char *model_name;
                if (gst_structure_get(s, "label", G_TYPE_STRING, &label, "model_name", G_TYPE_STRING, &model_name,
                                      NULL)) {
                    const gchar *attribute_name = gst_structure_has_field(s, "attribute_name")
                                                      ? gst_structure_get_string(s, "attribute_name")
                                                      : s_name;
                    jobject.push_back(json::object_t::value_type(
                        attribute_name, {{"label", label}, {"model", {{"name", model_name}}}}));
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
            res.push_back(jobject);
        }
    }
    return res;
}

/**
 * @return JSON array which contains raw tensor metas from frame.
 */
json convert_frame_tensors(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    GVA::VideoFrame video_frame(buffer, converter->info);
    const std::vector<GVA::Tensor> tensors = video_frame.tensors();
    json array = json::array();
    for (auto &tensor : video_frame.tensors()) {
        if (!tensor.has_field("type")) {
            array.push_back(convert_tensor(tensor));
        }
    }
    return array;
}

/**
 * @return JSON object which contains full-frame attributes and full-frame classification results from frame.
 */
json convert_frame_classification(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    GVA::VideoFrame video_frame(buffer, converter->info);
    const std::vector<GVA::Tensor> tensors = video_frame.tensors();
    /* check if there is any full-frame classification tensors attached to the buffer */
    if (!std::any_of(tensors.cbegin(), tensors.cend(),
                     [](const GVA::Tensor &tensor) { return tensor.get_string("type") == "classification_result"; })) {
        return json{};
    }

    json jobject = json::object();
    if (converter->add_tensor_data) {
        jobject["tensors"] = json::array();
    }
    jobject.push_back({"x", 0});
    jobject.push_back({"y", 0});
    jobject.push_back({"w", converter->info->width});
    jobject.push_back({"h", converter->info->height});

    for (GVA::Tensor &tensor : video_frame.tensors()) {
        if (tensor.get_string("type") != "classification_result")
            continue;
        if (tensor.has_field("label") && tensor.has_field("model_name")) {
            std::string label = tensor.label();
            std::string model_name = tensor.model_name();
            std::string attribute_name =
                tensor.has_field("attribute_name") ? tensor.get_string("attribute_name") : tensor.name();
            jobject.push_back(
                json::object_t::value_type(attribute_name, {{"label", label}, {"model", {{"name", model_name}}}}));
        }
        if (converter->add_tensor_data) {
            jobject["tensors"].push_back(convert_tensor(tensor));
        }
    }
    return jobject;
}

gboolean to_json(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    GST_DEBUG_CATEGORY_INIT(gst_json_converter_debug, "jsonconverter", 0, "JSON converter");
    try {
        if (converter->info) {
            json jframe = get_frame_data(converter, buffer);
            /* objects section */
            json jframe_objects;
            json roi_detection = convert_roi_detection(converter, buffer);
            if (!roi_detection.empty()) {
                jframe_objects = roi_detection;
            } /* roi_detection can contain multiple objects, while frame_classification - only one */
            json frame_classification = convert_frame_classification(converter, buffer);
            if (!frame_classification.empty()) {
                jframe_objects.push_back(frame_classification);
            }
            /* tensors section */
            json jframe_tensors;
            if (converter->add_tensor_data) {
                jframe_tensors = convert_frame_tensors(converter, buffer);
            }

            if (jframe_objects.empty() && jframe_tensors.empty()) {
                if (!converter->add_empty_detection_results) {
                    GST_DEBUG_OBJECT(converter, "No detections found. Not posting JSON message");
                    return TRUE;
                }
            }

            if (!jframe.is_null()) {
                if (!jframe_objects.empty()) {
                    jframe["objects"] = jframe_objects;
                }
                if (!jframe_tensors.empty()) {
                    jframe["tensors"] = jframe_tensors;
                }
                std::string json_message = jframe.dump(converter->json_indent);
                GVA::VideoFrame video_frame(buffer, converter->info);
                video_frame.add_message(json_message);
                GST_INFO_OBJECT(converter, "JSON message: %s", json_message.c_str());
            }
        }
#ifdef AUDIO
        else {
            return convert_audio_meta_to_json(converter, buffer);
        }
#endif
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(converter, "%s", e.what());
        return FALSE;
    }
    return TRUE;
}