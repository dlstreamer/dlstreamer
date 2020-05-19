/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <gst/gst.h>

#include "gstgvadetect.h"
#include "inference_backend/logger.h"
#include "inference_backend/safe_arithmetic.h"
#include "post_processors.h"
#include "video_frame.h"

#define UNUSED(x) (void)(x)

static void get_label_by_label_id(GstStructure *detection_tensor, int label_id, gchar **out_label) {
    *out_label = nullptr;
    GValueArray *labels = nullptr;
    if (detection_tensor && gst_structure_get_array(detection_tensor, "labels", &labels)) {
        if (labels && label_id >= 0 && label_id < (gint)labels->n_values) {
            *out_label = g_value_dup_string(labels->values + label_id);
        }
        g_value_array_free(labels);
    }
}

static void clip_normalized_rect(double &x, double &y, double &w, double &h) {
    if (!((x >= 0) && (y >= 0) && (w >= 0) && (h >= 0) && (x + w <= 1) && (y + h <= 1))) {
        GST_DEBUG("ROI coordinates x=[%.5f, %.5f], y=[%.5f, %.5f] are out of range [0,1] and will be clipped", x, x + w,
                  y, y + h);

        x = (x < 0) ? 0 : (x > 1) ? 1 : x;
        y = (y < 0) ? 0 : (y > 1) ? 1 : y;
        w = (w < 0) ? 0 : (w > 1 - x) ? 1 - x : w;
        h = (h < 0) ? 0 : (h > 1 - y) ? 1 - y : h;
    }
}

static void add_roi(GstBuffer *buffer, GstVideoInfo *info, double x, double y, double w, double h, int label_id,
                    double confidence, GstStructure *detection_tensor) {
    clip_normalized_rect(x, y, w, h);

    gchar *label = nullptr;
    get_label_by_label_id(detection_tensor, label_id, &label);

    uint32_t _x = safe_convert<uint32_t>(x * info->width + 0.5);
    uint32_t _y = safe_convert<uint32_t>(y * info->height + 0.5);
    uint32_t _w = safe_convert<uint32_t>(w * info->width + 0.5);
    uint32_t _h = safe_convert<uint32_t>(h * info->height + 0.5);
    GstVideoRegionOfInterestMeta *meta = gst_buffer_add_video_region_of_interest_meta(buffer, label, _x, _y, _w, _h);
    g_free(label);
    if (not meta)
        throw std::runtime_error("Failed to add GstVideoRegionOfInterestMeta to buffer");

    gst_structure_set_name(detection_tensor, "detection"); // make sure name="detection"
    gst_structure_set(detection_tensor, "label_id", G_TYPE_INT, label_id, "confidence", G_TYPE_DOUBLE, confidence,
                      "x_min", G_TYPE_DOUBLE, x, "x_max", G_TYPE_DOUBLE, x + w, "y_min", G_TYPE_DOUBLE, y, "y_max",
                      G_TYPE_DOUBLE, y + h, NULL);
    gst_video_region_of_interest_meta_add_param(meta, detection_tensor);
}

namespace Yolo {
struct DetectedObject {
    gfloat x;
    gfloat y;
    gfloat w;
    gfloat h;
    guint class_id;
    gfloat confidence;

    DetectedObject(gfloat x, gfloat y, gfloat w, gfloat h, guint class_id, gfloat confidence, gfloat h_scale = 1.f,
                   gfloat w_scale = 1.f) {
        this->x = (x - w / 2) * w_scale;
        this->y = (y - h / 2) * h_scale;
        this->w = w * w_scale;
        this->h = h * h_scale;
        this->class_id = class_id;
        this->confidence = confidence;
    }

    bool operator<(const DetectedObject &other) const {
        return this->confidence < other.confidence;
    }

    bool operator>(const DetectedObject &other) const {
        return this->confidence > other.confidence;
    }
};

void run_nms(std::vector<DetectedObject> &candidates, gdouble threshold) {
    ITT_TASK(__FUNCTION__);
    std::sort(candidates.rbegin(), candidates.rend());

    for (auto p_first_candidate = candidates.begin(); p_first_candidate != candidates.end(); ++p_first_candidate) {
        const auto &first_candidate = *p_first_candidate;
        double first_candidate_area = first_candidate.w * first_candidate.h;

        for (auto p_candidate = p_first_candidate + 1; p_candidate != candidates.end();) {
            const auto &candidate = *p_candidate;

            gdouble inter_width = std::min(first_candidate.x + first_candidate.w, candidate.x + candidate.w) -
                                  std::max(first_candidate.x, candidate.x);
            gdouble inter_height = std::min(first_candidate.y + first_candidate.h, candidate.y + candidate.h) -
                                   std::max(first_candidate.y, candidate.y);
            if (inter_width <= 0.0 || inter_height <= 0.0) {
                ++p_candidate;
                continue;
            }

            gdouble inter_area = inter_width * inter_height;
            gdouble candidate_area = candidate.w * candidate.h;

            gdouble overlap = inter_area / (candidate_area + first_candidate_area - inter_area);
            if (overlap > threshold)
                p_candidate = candidates.erase(p_candidate);
            else
                ++p_candidate;
        }
    }
}

void storeObjects(std::vector<DetectedObject> &objects, const InferenceFrame &frame, GstStructure *detection_result) {
    ITT_TASK(__FUNCTION__);
    double nms_threshold = 0.5;
    gst_structure_get_double(detection_result, "nms_threshold", &nms_threshold);
    run_nms(objects, nms_threshold);

    for (DetectedObject &object : objects) {
        add_roi(frame.buffer, frame.info, object.x, object.y, object.w, object.h, object.class_id, object.confidence,
                gst_structure_copy(detection_result)); // each ROI gets its own copy, which is then
                                                       // owned by GstVideoRegionOfInterestMeta
    }
}

namespace V2Tiny {

const size_t NUMBER_OF_CLASSES = 20;

enum RawNetOut : size_t {
    INDEX_X = 0,
    INDEX_Y = 1,
    INDEX_W = 2,
    INDEX_H = 3,
    INDEX_SCALE = 4,
    INDEX_CLASS_PROB_BEGIN = 5,
    INDEX_CLASS_PROB_END = INDEX_CLASS_PROB_BEGIN + NUMBER_OF_CLASSES,
    INDEX_COUNT = INDEX_CLASS_PROB_END
};

const size_t K_ANCHOR_SN = 13;
constexpr size_t K_OUT_BLOB_ITEM_N = 25;
constexpr size_t K_2_DEPTHS = K_ANCHOR_SN * K_ANCHOR_SN;
constexpr size_t K_3_DEPTHS = K_2_DEPTHS * K_OUT_BLOB_ITEM_N;

bool TensorToBBox(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                  std::vector<InferenceFrame> frames, GstStructure *detection_result) {
    ITT_TASK(__FUNCTION__);
    if (frames.size() != 1) {
        std::string err = "Batch size other than 1 is not supported";
        const gchar *converter = gst_structure_get_string(detection_result, "converter");
        if (converter)
            err += " for this post processor: " + std::string(converter);
        throw std::invalid_argument(err);
    }
    GstGvaDetect *gva_detect = (GstGvaDetect *)frames[0].gva_base_inference;
    if (not gva_detect)
        throw std::invalid_argument("gva_base_inference attached to inference frames is nullptr");

    static const float K_ANCHOR_SCALES[] = {1.08f, 1.19f, 3.42f, 4.41f, 6.63f, 11.38f, 9.42f, 5.11f, 16.62f, 10.52f};
    std::vector<Yolo::DetectedObject> objects;
    for (const auto &blob_iter : output_blobs) {
        InferenceBackend::OutputBlob::Ptr blob = blob_iter.second;
        if (not blob)
            throw std::invalid_argument("Output blob is nullptr");

        const float *blob_data = (const float *)blob->GetData();
        if (not blob_data)
            throw std::invalid_argument("Output blob data is nullptr");

        float raw_netout[INDEX_COUNT];
        for (size_t k = 0; k < 5; k++) {
            float anchor_w = K_ANCHOR_SCALES[k * 2];
            float anchor_h = K_ANCHOR_SCALES[k * 2 + 1];

            for (size_t i = 0; i < K_ANCHOR_SN; i++) {
                for (size_t j = 0; j < K_ANCHOR_SN; j++) {
                    const size_t common_offset = k * K_3_DEPTHS + i * K_ANCHOR_SN + j;

                    raw_netout[INDEX_X] = blob_data[common_offset + 0 * K_2_DEPTHS];     // x
                    raw_netout[INDEX_Y] = blob_data[common_offset + 1 * K_2_DEPTHS];     // y
                    raw_netout[INDEX_W] = blob_data[common_offset + 2 * K_2_DEPTHS];     // w
                    raw_netout[INDEX_H] = blob_data[common_offset + 3 * K_2_DEPTHS];     // h
                    raw_netout[INDEX_SCALE] = blob_data[common_offset + 4 * K_2_DEPTHS]; // scale

                    for (size_t i = INDEX_CLASS_PROB_BEGIN; i < INDEX_CLASS_PROB_END; ++i) {
                        raw_netout[i] =
                            blob_data[common_offset + i * K_2_DEPTHS] * raw_netout[INDEX_SCALE]; // scaled probs
                        if (raw_netout[i] <= gva_detect->threshold)
                            raw_netout[i] = 0.f;
                    }

                    std::pair<gint, float> max_info = std::make_pair(0, 0.f);
                    for (size_t l = INDEX_CLASS_PROB_BEGIN; l < INDEX_CLASS_PROB_END; l++) {
                        float class_prob = raw_netout[l];
                        if (class_prob > 1.f) {
                            GST_WARNING_OBJECT(gva_detect, "class_prob weird %f", class_prob);
                        }
                        if (class_prob > max_info.second) {
                            max_info.first = l - INDEX_CLASS_PROB_BEGIN;
                            max_info.second = class_prob;
                        }
                    }

                    if (max_info.second > gva_detect->threshold) {
                        // scale back to image width/height
                        float cx = (j + raw_netout[INDEX_X]) / K_ANCHOR_SN;
                        float cy = (i + raw_netout[INDEX_Y]) / K_ANCHOR_SN;
                        float w = std::exp(raw_netout[INDEX_W]) * anchor_w / K_ANCHOR_SN;
                        float h = std::exp(raw_netout[INDEX_H]) * anchor_h / K_ANCHOR_SN;
                        DetectedObject object(cx, cy, w, h, max_info.first, max_info.second);
                        objects.push_back(object);
                    }
                }
            }
        }
    }
    storeObjects(objects, frames[0], detection_result);
    return true;
}

} // namespace V2Tiny

namespace V3 {

uint32_t EntryIndex(uint32_t side, uint32_t lcoords, uint32_t lclasses, uint32_t location, uint32_t entry) {
    uint32_t side_square = side * side;
    uint32_t n = location / side_square;
    uint32_t loc = location % side_square;
    // side_square is the tensor dimension of the YoloV3 model. Overflow is not possible here.
    return side_square * (n * (lcoords + lclasses + 1) + entry) + loc;
}

bool TensorToBBox(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                  std::vector<InferenceFrame> frames, GstStructure *detection_result) {
    ITT_TASK(__FUNCTION__);
    try {
        if (frames.size() != 1) {
            std::string err = "Batch size other than 1 is not supported";
            const gchar *converter = gst_structure_get_string(detection_result, "converter");
            if (converter)
                err += " for this post processor: " + std::string(converter);
            throw std::invalid_argument(err);
        }
        GstGvaDetect *gva_detect = (GstGvaDetect *)frames[0].gva_base_inference;
        if (not gva_detect)
            throw std::invalid_argument("gva_base_inference attached to inference frames is nullptr");

        std::vector<Yolo::DetectedObject> objects;
        constexpr int coords = 4;
        constexpr int num = 3;
        constexpr int classes = 80;
        constexpr float input_size = 416;
        constexpr float anchors[] = {10.0f, 13.0f, 16.0f,  30.0f,  33.0f, 23.0f,  30.0f,  61.0f,  62.0f,
                                     45.0f, 59.0f, 119.0f, 116.0f, 90.0f, 156.0f, 198.0f, 373.0f, 326.0f};
        for (const auto &blob_iter : output_blobs) {
            InferenceBackend::OutputBlob::Ptr blob = blob_iter.second;
            if (not blob)
                throw std::invalid_argument("Output blob is nullptr");

            auto dims = blob->GetDims();
            if (dims.size() != 4 or dims[2] != dims[3]) {
                throw std::runtime_error("Invalid output blob dimensions");
            }
            const int side = dims[2];
            int anchor_offset = 0;
            switch (side) {
            case 13:
                anchor_offset = 2 * 6;
                break;
            case 26:
                anchor_offset = 2 * 3;
                break;
            case 52:
                anchor_offset = 2 * 0;
                break;
            default:
                throw std::runtime_error("Invalid output size dimension");
            }

            const float *output_blob = (const float *)blob->GetData();
            if (not output_blob)
                throw std::invalid_argument("Output blob data is nullptr");

            const uint32_t side_square = side * side;
            for (uint32_t i = 0; i < side_square; ++i) {
                const int row = i / side;
                const int col = i % side;
                for (uint32_t n = 0; n < num; ++n) {
                    const int obj_index = Yolo::V3::EntryIndex(side, coords, classes, n * side_square + i, coords);
                    const int box_index = Yolo::V3::EntryIndex(side, coords, classes, n * side_square + i, 0);

                    const float scale = output_blob[obj_index];
                    if (scale < gva_detect->threshold)
                        continue;
                    // TODO: check if index in array range
                    const float x = (col + output_blob[box_index + 0 * side_square]) / side * input_size;
                    const float y = (row + output_blob[box_index + 1 * side_square]) / side * input_size;

                    // TODO: check if index in array range
                    const float width =
                        std::exp(output_blob[box_index + 2 * side_square]) * anchors[anchor_offset + 2 * n];
                    const float height =
                        std::exp(output_blob[box_index + 3 * side_square]) * anchors[anchor_offset + 2 * n + 1];

                    for (uint32_t j = 0; j < classes; ++j) {
                        const int class_index =
                            Yolo::V3::EntryIndex(side, coords, classes, n * side_square + i, coords + 1 + j);
                        const float prob = scale * output_blob[class_index];
                        if (prob < gva_detect->threshold)
                            continue;
                        Yolo::DetectedObject obj(x, y, width, height, j, prob, 1 / input_size, 1 / input_size);
                        objects.push_back(obj);
                    }
                }
            }
        }
        Yolo::storeObjects(objects, frames[0], detection_result);

        return true;
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do YoloV3 post-processing"));
    }
}

} // namespace V3
} // namespace Yolo

namespace SSD {

gboolean TensorToBBox(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                      std::vector<InferenceFrame> frames, GstStructure *detection_result) {
    ITT_TASK(__FUNCTION__);
    try {
        if (not detection_result)
            throw std::invalid_argument("detection_result tensor is nullptr");

        // Read labels and roi_scale from GstStructure config
        GValueArray *labels_raw = nullptr;
        gdouble roi_scale = 1.0;
        gst_structure_get_array(detection_result, "labels", &labels_raw);
        gst_structure_get_double(detection_result, "roi_scale", &roi_scale);

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        auto labels = std::unique_ptr<GValueArray, decltype(&g_value_array_free)>(labels_raw, g_value_array_free);
        G_GNUC_END_IGNORE_DEPRECATIONS
        // Check whether we can handle this blob instead iterator
        for (const auto &blob_iter : output_blobs) {
            InferenceBackend::OutputBlob::Ptr blob = blob_iter.second;
            if (not blob)
                throw std::invalid_argument("Output blob is nullptr");

            const float *data = (const float *)blob->GetData();
            if (not data)
                throw std::invalid_argument("Output blob data is nullptr");

            auto dims = blob->GetDims();
            guint dims_size = dims.size();

            static constexpr guint min_dims_size = 2;
            if (dims_size < min_dims_size)
                throw std::invalid_argument("Output blob dimentions size " + std::to_string(dims_size) +
                                            " is not supported (less than " + std::to_string(min_dims_size) + ")");

            for (guint i = min_dims_size + 1; i < dims_size; ++i) {
                if (dims[dims_size - i] != 1)
                    throw std::invalid_argument(
                        "All output blob dimensions, except for object size and max objects count, must be equal to 1");
            }

            guint object_size = dims[dims_size - 1];
            static constexpr guint supported_object_size = 7; // SSD DetectionOutput format
            if (object_size != supported_object_size)
                throw std::invalid_argument("Object size dimension of output blob is set to " +
                                            std::to_string(object_size) + ", but only " +
                                            std::to_string(supported_object_size) + " supported");

            GstVideoInfo video_info;
            guint max_proposal_count = dims[dims_size - 2];
            for (guint i = 0; i < max_proposal_count; ++i) {
                gint image_id = (gint)data[i * object_size + 0];
                gint label_id = (gint)data[i * object_size + 1];
                gdouble confidence = data[i * object_size + 2];
                gdouble x_min = data[i * object_size + 3];
                gdouble y_min = data[i * object_size + 4];
                gdouble x_max = data[i * object_size + 5];
                gdouble y_max = data[i * object_size + 6];

                // check image_id
                if (image_id < 0 || (size_t)image_id >= frames.size()) {
                    break;
                }

                // check confidence
                GstGvaDetect *gva_detect = (GstGvaDetect *)frames[image_id].gva_base_inference;
                if (not gva_detect)
                    throw std::invalid_argument("gva_base_inference attached to inference frames is nullptr");
                if (confidence < gva_detect->threshold) {
                    continue;
                }

                // This post processing happens not in main gstreamer thread (but in separate one). Thus, we can run
                // into a problem where buffer's gva_base_inference is already cleaned up (all frames are pushed from
                // decoder), so we can't use gva_base_inference's GstVideoInfo to get width and height. In this case we
                // use w and h of InferenceFrame to make VideoFrame box adding logic work correctly
                if (frames[image_id].gva_base_inference->info) {
                    video_info = *frames[image_id].gva_base_inference->info;
                } else {
                    video_info.width = frames[image_id].roi.w;
                    video_info.height = frames[image_id].roi.h;
                }
                GVA::VideoFrame video_frame(frames[image_id].buffer, frames[image_id].info);

                // TODO: check if we can simplify below code further
                // apply roi_scale if set
                if (roi_scale > 0 && roi_scale != 1) {
                    gdouble x_center = (x_max + x_min) * 0.5;
                    gdouble y_center = (y_max + y_min) * 0.5;
                    gdouble new_w = (x_max - x_min) * roi_scale;
                    gdouble new_h = (y_max - y_min) * roi_scale;
                    x_min = x_center - new_w * 0.5;
                    x_max = x_center + new_w * 0.5;
                    y_min = y_center - new_h * 0.5;
                    y_max = y_center + new_h * 0.5;
                }

                add_roi(frames[image_id].buffer, frames[image_id].info, x_min, y_min, x_max - x_min, y_max - y_min,
                        label_id, confidence,
                        gst_structure_copy(detection_result)); // each ROI gets its own copy, which is then
                                                               // owned by GstVideoRegionOfInterestMeta
            }
        }
        return true;
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do SSD post-processing"));
    }
}
} // namespace SSD
namespace {

bool ConvertBlobToDetectionResults(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                                   std::vector<InferenceFrame> frames, GstStructure *detection_result) {
    ITT_TASK(__FUNCTION__);
    if (output_blobs.empty())
        throw std::invalid_argument("There are no output blobs");
    if (not detection_result)
        throw std::invalid_argument("Detection result tensor is empty");

    std::string converter = "";
    if (gst_structure_has_field(detection_result, "converter")) {
        converter = (std::string)gst_structure_get_string(detection_result, "converter");
    } else {
        converter = "tensor_to_bbox_ssd";
        gst_structure_set(detection_result, "converter", G_TYPE_STRING, converter.c_str(), NULL);
    }
    static std::map<std::string, std::function<bool(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &,
                                                    std::vector<InferenceFrame>, GstStructure *)>>
        do_conversion{{"tensor_to_bbox_ssd", SSD::TensorToBBox}, // default post processing
                      {"DetectionOutput", SSD::TensorToBBox},    // GVA plugin R1.2 backward compatibility
                      {"tensor_to_bbox_yolo_v2_tiny", Yolo::V2Tiny::TensorToBBox},
                      {"tensor_to_bbox_yolo_v3", Yolo::V3::TensorToBBox}};
    if (do_conversion.find(converter) == do_conversion.end()) {
        // Wrong converter set in model-proc file
        std::string valid_converters = "";
        for (auto converter_iter = do_conversion.begin(); converter_iter != do_conversion.end(); ++converter_iter) {
            valid_converters += "\"" + converter_iter->first + "\"";
            if (std::next(converter_iter) != do_conversion.end()) // last element
                valid_converters += ", ";
        }
        throw std::invalid_argument(
            "Unknown post processing converter set: '" + converter +
            "'. Please set 'converter' field in model-proc file to one of the following values: " + valid_converters);
    }
    return do_conversion[converter](output_blobs, frames, detection_result);
}

void ExtractDetectionResults(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                             std::vector<InferenceFrame> frames,
                             const std::map<std::string, GstStructure *> &model_proc, const gchar *model_name) {
    ITT_TASK(__FUNCTION__);
    try {
        if (output_blobs.empty())
            throw std::invalid_argument("There are no output blobs");
        std::string layer_name = "";
        auto detection_result =
            std::unique_ptr<GstStructure, decltype(&gst_structure_free)>(nullptr, gst_structure_free);
        if (model_proc.empty()) {
            layer_name = output_blobs.cbegin()->first;
            detection_result.reset(gst_structure_new_empty("detection"));
        } else {
            // TODO: To delete this loop, it is necessary to pass to the function not the entire model_proc, which
            // contains both the preprocessing and postprocessing information, but separately the postprocessing
            // information.
            for (const auto &layer_out : output_blobs) {
                if (model_proc.find(layer_out.first) != model_proc.cend()) {
                    layer_name = layer_out.first;
                    detection_result.reset(gst_structure_copy(model_proc.at(layer_name)));
                    gst_structure_set_name(detection_result.get(), "detection");
                }
            }
            if (not detection_result) {
                detection_result.reset(gst_structure_new_empty("detection"));
                if (not detection_result.get())
                    throw std::runtime_error("Failed to create GstStructure with 'detection' name");
                layer_name = output_blobs.cbegin()->first;
            }
        }
        gst_structure_set(detection_result.get(), "layer_name", G_TYPE_STRING, layer_name.c_str(), "model_name",
                          G_TYPE_STRING, model_name, NULL);

        ConvertBlobToDetectionResults(output_blobs, frames, detection_result.get());
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to extract detection results"));
    }
}
} // namespace

PostProcFunction EXTRACT_DETECTION_RESULTS = ExtractDetectionResults;
