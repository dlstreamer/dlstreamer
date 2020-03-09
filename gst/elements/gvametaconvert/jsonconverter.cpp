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
#include <json.hpp>

using json = nlohmann::json;

json get_frame_data(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    json res;
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

json convert_roi_detection(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    json res;
    GVA::VideoFrame video_frame(buffer, converter->info);
    std::vector<GVA::RegionOfInterest> regions = video_frame.regions();
    for (std::vector<GVA::RegionOfInterest>::iterator it = regions.begin(); it != regions.end(); ++it) {
        gint id = 0;
        get_object_id(it->meta(), &id);

        json jobject = json::object();
        jobject.push_back({"x", it->meta()->x});
        jobject.push_back({"y", it->meta()->y});
        jobject.push_back({"w", it->meta()->w});
        jobject.push_back({"h", it->meta()->h});

        if (id != 0)
            jobject.push_back({"id", id});

        const gchar *roi_type = g_quark_to_string(it->meta()->roi_type);

        if (roi_type) {
            jobject.push_back({"roi_type", roi_type});
        }
        for (GList *l = it->meta()->params; l; l = g_list_next(l)) {

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
                                      G_TYPE_DOUBLE, &yminval, "y_max", G_TYPE_DOUBLE, &ymaxval, "confidence",
                                      G_TYPE_DOUBLE, &confidence, "label_id", G_TYPE_INT, &label_id, NULL)) {
                    json detection = {
                        {"bounding_box",
                         {{"x_min", xminval}, {"x_max", xmaxval}, {"y_min", yminval}, {"y_max", ymaxval}}},
                        {"confidence", confidence},
                        {"label_id", label_id}};

                    const gchar *label = g_quark_to_string(it->meta()->roi_type);

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
                    const gchar *attribute_name = gst_structure_get_string(s, "attribute_name")
                                                      ? gst_structure_get_string(s, "attribute_name")
                                                      : s_name;
                    jobject.push_back(json::object_t::value_type(
                        attribute_name, {{"label", label}, {"model", {{"name", model_name}}}}));
                }
            }
        }
        if (!jobject.empty()) {
            if (res["objects"].is_null()) {
                res["objects"] = json::array();
            }
            res["objects"].push_back(jobject);
        }
    }
    return res;
}

json convert_roi_tensor(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    json res;
    GVA::VideoFrame video_frame(buffer, converter->info);
    std::vector<GVA::RegionOfInterest> regions = video_frame.regions();
    for (std::vector<GVA::RegionOfInterest>::iterator it = regions.begin(); it != regions.end(); ++it) {
        for (GList *l = it->meta()->params; l; l = g_list_next(l)) {

            json jobject = json::object();
            GstStructure *s = (GstStructure *)l->data;
            GVA::Tensor s_tensor = GVA::Tensor(s);
            std::string precision_value = s_tensor.precision_as_string();
            if (!precision_value.empty()) {
                jobject.push_back(json::object_t::value_type("precision", precision_value));
            }
            std::string layout_value = s_tensor.layout_as_string();
            if (!layout_value.empty()) {
                jobject.push_back(json::object_t::value_type("layout", layout_value));
            }
            std::string name_value = s_tensor.name();
            if (!name_value.empty()) {
                jobject.push_back(json::object_t::value_type("name", name_value));
            }
            std::string model_name_value = s_tensor.model_name();
            if (!model_name_value.empty()) {
                jobject.push_back(json::object_t::value_type("model_name", model_name_value));
            }
            std::string layer_name_value = s_tensor.layer_name();
            if (!layer_name_value.empty()) {
                jobject.push_back(json::object_t::value_type("layer_name", layer_name_value));
            }
            std::string format_value = s_tensor.format();
            if (!format_value.empty()) {
                jobject.push_back(json::object_t::value_type("format", format_value));
            }
            if (!s_tensor.is_detection()) {
                std::string label_value = s_tensor.label();
                if (!label_value.empty()) {
                    jobject.push_back(json::object_t::value_type("label", label_value));
                }
            }
            double confidence_value;
            if (gst_structure_get_double(s, "confidence", &confidence_value)) {
                jobject.push_back(json::object_t::value_type("confidence", confidence_value));
            }
            int label_id_value;
            if (gst_structure_get_int(s, "label_id", &label_id_value)) {
                jobject.push_back(json::object_t::value_type("label_id", label_id_value));
            }
            json data_array;
            if (s_tensor.precision() == GVAPrecision::U8) {
                const std::vector<uint8_t> data = s_tensor.data<uint8_t>();
                for (guint i = 0; i < data.size(); i++) {
                    data_array += data[i];
                }
            } else {
                const std::vector<float> data = s_tensor.data<float>();
                for (guint i = 0; i < data.size(); i++) {
                    data_array += data[i];
                }
            }
            if (!data_array.is_null()) {
                jobject.push_back(json::object_t::value_type("data", data_array));
            }
            if (!jobject.empty()) {
                if (res["tensors"].is_null()) {
                    res["tensors"] = json::array();
                }
                res["tensors"].push_back(jobject);
            }
        }
    }
    return res;
}

void all_to_json(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    json jframe = get_frame_data(converter, buffer);
    json jroi_detection = convert_roi_detection(converter, buffer);
    json jroi_tensor = convert_roi_tensor(converter, buffer);
    if (jroi_detection.is_null()) {
        if (!converter->include_no_detections) {
            GST_DEBUG("No detections found. Not posting JSON message");
            return;
        }
    } else {
        jframe.update(jroi_detection);
    }
    if (!jroi_tensor.is_null()) {
        jframe.update(jroi_tensor);
    }
    if (!jframe.is_null()) {
        std::string json_message = jframe.dump();
        GVA::VideoFrame video_frame(buffer, converter->info);
        video_frame.add_message(json_message);
        GST_INFO("JSON message: %s", json_message.c_str());
    }
}

void detection_to_json(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    json jframe = get_frame_data(converter, buffer);
    json jroi_detection = convert_roi_detection(converter, buffer);
    if (jroi_detection.is_null()) {
        if (!converter->include_no_detections) {
            GST_DEBUG("No detections found. Not posting JSON message");
            return;
        }
    } else {
        jframe.update(jroi_detection);
    }
    if (!jframe.is_null()) {
        std::string json_message = jframe.dump();
        GVA::VideoFrame video_frame(buffer, converter->info);
        video_frame.add_message(json_message);
        GST_INFO("JSON message: %s", json_message.c_str());
    }
}

void tensor_to_json(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    json jframe = get_frame_data(converter, buffer);
    json jroi_tensor = convert_roi_tensor(converter, buffer);
    if (!jroi_tensor.is_null()) {
        jframe.update(jroi_tensor);
    }
    if (!jframe.is_null()) {
        std::string json_message = jframe.dump();
        GVA::VideoFrame video_frame(buffer, converter->info);
        video_frame.add_message(json_message);
        GST_INFO("JSON message: %s", json_message.c_str());
    }
}
