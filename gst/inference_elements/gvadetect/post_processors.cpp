/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
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
#include "post_processors.h"
#include "post_processors_util.h"

namespace {

using namespace InferenceBackend;

gboolean TensorToBBoxSSD(const InferenceBackend::OutputBlob::Ptr &blob, std::vector<InferenceROI> frames,
                         GstStructure *layer_post_proc) {
    const float *data = (const float *)blob->GetData();
    if (data == nullptr) {
        GST_ERROR("Blob data pointer is null");
        return false;
    }
    const gchar *converter = gst_structure_get_string(layer_post_proc, "converter");

    // Check whether we can handle this blob
    auto dims = blob->GetDims();
    guint dims_size = dims.size();
    static const guint min_dims_size = 2;
    if (dims_size < min_dims_size) {
        GST_ERROR("Output blob converter '%s': Output blob with inference results has %d dimensions, but it should "
                  "have at least %d. Boxes won't be extracted\n",
                  converter, dims_size, min_dims_size);
        return false;
    }

    guint object_size = dims[dims_size - 1];
    guint max_proposal_count = dims[dims_size - 2];
    for (guint i = min_dims_size + 1; i < dims_size; ++i) {
        if (dims[dims_size - i] != 1) {
            GST_ERROR("Output blob converter '%s': All output blob dimensions, except object size and max objects "
                      "count, must be equal to 1. Boxes won't be extracted\n",
                      converter);
            return false;
        }
    }

    static const guint supported_object_size = 7;
    if (object_size != supported_object_size) { // SSD DetectionOutput format
        GST_ERROR("Output blob converter '%s': Object size dimension of output blob is set to %d and doesn't equal to "
                  "supported - %d. Boxes won't be extracted\n",
                  converter, object_size, supported_object_size);
        return false;
    }

    // Read labels and roi_scale from GstStructure config
    GValueArray *labels = nullptr;
    gdouble roi_scale = 1.0;
    gst_structure_get_array(layer_post_proc, "labels", &labels);
    gst_structure_get_double(layer_post_proc, "roi_scale", &roi_scale);

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
        if (confidence < gva_detect->threshold) {
            continue;
        }

        gint width = frames[image_id].roi.w;
        gint height = frames[image_id].roi.h;

        // get label
        const gchar *label = NULL;
        if (labels && label_id >= 0 && label_id < (gint)labels->n_values) {
            label = g_value_get_string(labels->values + label_id);
        }

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

        gint ix_min = (gint)(x_min * width + 0.5);
        gint iy_min = (gint)(y_min * height + 0.5);
        gint ix_max = (gint)(x_max * width + 0.5);
        gint iy_max = (gint)(y_max * height + 0.5);
        if (ix_min < 0)
            ix_min = 0;
        if (iy_min < 0)
            iy_min = 0;
        if (ix_max > width)
            ix_max = width;
        if (iy_max > height)
            iy_max = height;
        GstVideoRegionOfInterestMeta *meta = gst_buffer_add_video_region_of_interest_meta(
            frames[image_id].buffer, label, ix_min, iy_min, ix_max - ix_min, iy_max - iy_min);

        GstStructure *s = gst_structure_copy(layer_post_proc);
        gst_structure_set(s, "confidence", G_TYPE_DOUBLE, confidence, "label_id", G_TYPE_INT, label_id, "x_min",
                          G_TYPE_DOUBLE, x_min, "x_max", G_TYPE_DOUBLE, x_max, "y_min", G_TYPE_DOUBLE, y_min, "y_max",
                          G_TYPE_DOUBLE, y_max, NULL);
        gst_video_region_of_interest_meta_add_param(meta, s);
    }

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    if (labels)
        g_value_array_free(labels);
    G_GNUC_END_IGNORE_DEPRECATIONS
    return true;
}

namespace YoloV2Tiny {
struct DetectedObject {
    gfloat x;
    gfloat y;
    gfloat w;
    gfloat h;
    gint class_id;
    gfloat confidence;

    DetectedObject(gfloat x, gfloat y, gfloat w, gfloat h, gint class_id, gfloat confidence, gfloat h_scale = 1.f,
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
    void clip() {
        this->x = (this->x < 0.0) ? 0.0 : (this->x > 1.0) ? 1.0 : this->x;
        this->y = (this->y < 0.0) ? 0.0 : (this->y > 1.0) ? 1.0 : this->y;
        this->w = (this->w < 0.0) ? 0.0 : (this->x + this->w > 1.0) ? 1.0 - this->x : this->w;
        this->h = (this->h < 0.0) ? 0.0 : (this->y + this->h > 1.0) ? 1.0 - this->y : this->h;
    }
};

std::vector<DetectedObject> run_nms(std::vector<DetectedObject> candidates, gdouble threshold) {
    std::vector<DetectedObject> nms_candidates;
    std::sort(candidates.rbegin(), candidates.rend());

    while (candidates.size() > 0) {
        auto p_first_candidate = candidates.begin();
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

            gdouble overlap = inter_area / std::min(candidate_area, first_candidate_area);
            if (overlap > threshold)
                p_candidate = candidates.erase(p_candidate);
            else
                ++p_candidate;
        }

        nms_candidates.push_back(first_candidate);
        candidates.erase(p_first_candidate);
    }

    return nms_candidates;
}

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

void fillRawNetOutMoviTL(const float *blob_data, const size_t anchor_index, const size_t cell_index,
                         const float threshold, float *converted_blob_data) {
    if (blob_data == nullptr || converted_blob_data == nullptr) {
        // TODO: log anything
        return;
    }

    static const size_t strides4D[] = {13 * 128, 128, 25, 1};
    const size_t offset = cell_index * strides4D[1] + anchor_index * strides4D[2];
    for (size_t i = 0; i < INDEX_COUNT; i++) {
        const size_t index = offset + i * strides4D[3];
        converted_blob_data[i] = dequantize((reinterpret_cast<const uint8_t *>(blob_data))[index]);
    }
    converted_blob_data[INDEX_X] = sigmoid(converted_blob_data[INDEX_X]);         // x
    converted_blob_data[INDEX_Y] = sigmoid(converted_blob_data[INDEX_Y]);         // y
    converted_blob_data[INDEX_SCALE] = sigmoid(converted_blob_data[INDEX_SCALE]); // scale

    softMax(converted_blob_data + INDEX_CLASS_PROB_BEGIN, INDEX_CLASS_PROB_END - INDEX_CLASS_PROB_BEGIN); // probs
    for (size_t i = INDEX_CLASS_PROB_BEGIN; i < INDEX_CLASS_PROB_END; ++i) {
        converted_blob_data[i] *= converted_blob_data[INDEX_SCALE]; // scaled probs
        if (converted_blob_data[i] <= threshold)
            converted_blob_data[i] = 0.f;
    }
}

void fillRawNetOut(const float *blob_data, const size_t anchor_index, const size_t cell_index, const float threshold,
                   float *converted_blob_data) {
    constexpr size_t K_OUT_BLOB_ITEM_N = 25;
    constexpr size_t K_2_DEPTHS = K_ANCHOR_SN * K_ANCHOR_SN;
    constexpr size_t K_3_DEPTHS = K_2_DEPTHS * K_OUT_BLOB_ITEM_N;

    const size_t common_offset = anchor_index * K_3_DEPTHS + cell_index;

    converted_blob_data[INDEX_X] = blob_data[common_offset + 1 * K_2_DEPTHS]; // x
    converted_blob_data[INDEX_Y] = blob_data[common_offset + 0 * K_2_DEPTHS]; // y
    converted_blob_data[INDEX_W] = blob_data[common_offset + 3 * K_2_DEPTHS]; // w
    converted_blob_data[INDEX_H] = blob_data[common_offset + 2 * K_2_DEPTHS]; // h

    converted_blob_data[INDEX_SCALE] = blob_data[common_offset + 4 * K_2_DEPTHS]; // scale

    for (size_t i = INDEX_CLASS_PROB_BEGIN; i < INDEX_CLASS_PROB_END; ++i) {
        converted_blob_data[i] =
            blob_data[common_offset + i * K_2_DEPTHS] * converted_blob_data[INDEX_SCALE]; // scaled probs
        if (converted_blob_data[i] <= threshold)
            converted_blob_data[i] = 0.f;
    }
}

using RawNetOutExtractor = std::function<void(const float *, const size_t, const size_t, const float, float *)>;

bool TensorToBBoxYoloV2TinyCommon(const InferenceBackend::OutputBlob::Ptr &blob, std::vector<InferenceROI> frames,
                                  GstStructure *layer_post_proc, RawNetOutExtractor extractor) {
    const char *converter = gst_structure_get_string(layer_post_proc, "converter");

    if (frames.size() != 1) {
        GST_ERROR(
            "Batch size not equal to 1 is not supported for this post proc converter: '%s', boxes won't be extracted\n",
            converter);
        return false;
    }
    int image_width = frames[0].roi.w;
    int image_height = frames[0].roi.h;
    GstGvaDetect *gva_detect = (GstGvaDetect *)frames[0].gva_base_inference;

    static const float K_ANCHOR_SCALES[] = {1.08f, 1.19f, 3.42f, 4.41f, 6.63f, 11.38f, 9.42f, 5.11f, 16.62f, 10.52f};

    const float *data = (const float *)blob->GetData();
    if (data == nullptr) {
        GST_ERROR("Blob data pointer is null");
        return false;
    }

    float raw_netout[INDEX_COUNT];
    std::vector<DetectedObject> objects;
    for (size_t k = 0; k < 5; k++) {
        float anchor_w = K_ANCHOR_SCALES[k * 2];
        float anchor_h = K_ANCHOR_SCALES[k * 2 + 1];

        for (size_t i = 0; i < K_ANCHOR_SN; i++) {
            for (size_t j = 0; j < K_ANCHOR_SN; j++) {
                extractor(data, k, i * K_ANCHOR_SN + j, gva_detect->threshold, raw_netout);

                std::pair<gint, float> max_info = std::make_pair(0, 0.f);
                for (size_t l = INDEX_CLASS_PROB_BEGIN; l < INDEX_CLASS_PROB_END; l++) {
                    float class_prob = raw_netout[l];
                    if (class_prob > 1.f) {
                        GST_WARNING_OBJECT(gva_detect, "class_prob weired %f", class_prob);
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

    double nms_threshold = 0.5;
    gst_structure_get_double(layer_post_proc, "nms_threshold", &nms_threshold);
    objects = run_nms(objects, nms_threshold);

    GValueArray *labels = nullptr;

    gst_structure_get_array(layer_post_proc, "labels", &labels); // TODO: free in the end?

    const char *label = NULL;
    for (DetectedObject &object : objects) {
        object.clip();

        label = g_value_get_string(labels->values + object.class_id); // TODO: make me safe please!
        GstVideoRegionOfInterestMeta *meta = gst_buffer_add_video_region_of_interest_meta(
            frames[0].buffer, label, object.x * image_width, object.y * image_height, object.w * image_width,
            object.h * image_height);

        GstStructure *s = gst_structure_copy(layer_post_proc);
        gst_structure_set(s, "confidence", G_TYPE_DOUBLE, object.confidence, "label_id", G_TYPE_INT, object.class_id,
                          "x_min", G_TYPE_DOUBLE, object.x, "x_max", G_TYPE_DOUBLE, object.x + object.w, "y_min",
                          G_TYPE_DOUBLE, object.y, "y_max", G_TYPE_DOUBLE, object.y + object.h, NULL);

        gst_video_region_of_interest_meta_add_param(meta, s);
    }

    return true;
}

} // namespace YoloV2Tiny

bool TensorToBBoxYoloV2Tiny(const InferenceBackend::OutputBlob::Ptr &blob, std::vector<InferenceROI> frames,
                            GstStructure *layer_post_proc) {
    return YoloV2Tiny::TensorToBBoxYoloV2TinyCommon(blob, frames, layer_post_proc, YoloV2Tiny::fillRawNetOut);
}

bool TensorToBBoxYoloV2TinyMoviTL(const InferenceBackend::OutputBlob::Ptr &blob, std::vector<InferenceROI> frames,
                                  GstStructure *layer_post_proc) {
    return YoloV2Tiny::TensorToBBoxYoloV2TinyCommon(blob, frames, layer_post_proc, YoloV2Tiny::fillRawNetOutMoviTL);
}

bool ConvertBlobToDetectionResults(const InferenceBackend::OutputBlob::Ptr &blob, std::vector<InferenceROI> frames,
                                   GstStructure *layer_post_proc) {
    if (blob == nullptr)
        throw std::runtime_error("Blob is empty during post processing. Cannot access null object.");
    if (layer_post_proc == nullptr)
        throw std::runtime_error("Post proc layer is null during post processing. Cannot access null object.");

    std::string converter = "tensor_to_bbox_ssd"; // default post processing
    if (gst_structure_has_field(layer_post_proc, "converter"))
        converter = (std::string)gst_structure_get_string(layer_post_proc, "converter");
    else
        gst_structure_set(layer_post_proc, "converter", G_TYPE_STRING, converter.c_str(), NULL);

    static std::map<std::string, std::function<bool(const InferenceBackend::OutputBlob::Ptr &,
                                                    std::vector<InferenceROI>, GstStructure *)>>
        do_conversion{{"tensor_to_bbox_ssd", TensorToBBoxSSD}, // default post processing
                      {"DetectionOutput", TensorToBBoxSSD},    // GVA plugin R1.2 backward compatibility
                      {"tensor_to_bbox_yolo_v2_tiny", TensorToBBoxYoloV2Tiny},
                      {"tensor_to_bbox_yolo_v2_tiny_moviTL", TensorToBBoxYoloV2TinyMoviTL}};

    if (do_conversion.find(converter) == do_conversion.end()) {
        // Wrong converter set in model-proc file
        std::string valid_converters = "";
        for (auto converter_iter = do_conversion.begin(); converter_iter != do_conversion.end(); ++converter_iter) {
            valid_converters += "\"" + converter_iter->first + "\"";
            if (std::next(converter_iter) != do_conversion.end()) // last element
                valid_converters += ", ";
        }
        GST_ERROR(
            "Unknown post proc converter: \"%s\". Please set \"converter\" field in model-proc file to one of the "
            "following values: %s",
            converter.c_str(), valid_converters.c_str());
        return false;
    }

    return do_conversion[converter](blob, frames, layer_post_proc);
}

void ExtractDetectionResults(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                             std::vector<InferenceROI> frames, const std::map<std::string, GstStructure *> &model_proc,
                             const gchar *model_name) {
    for (const auto &blob_iter : output_blobs) {
        const std::string &layer_name = blob_iter.first;
        InferenceBackend::OutputBlob::Ptr blob = blob_iter.second;

        GstStructure *detection_result;
        const auto &post_proc = model_proc.find(layer_name);
        if (post_proc != model_proc.end()) {
            detection_result = gst_structure_copy(post_proc->second);
            gst_structure_set_name(detection_result, "detection");
        } else {
            detection_result = gst_structure_new_empty("detection");
        }
        gst_structure_set(detection_result, "layer_name", G_TYPE_STRING, layer_name.c_str(), "model_name",
                          G_TYPE_STRING, model_name, NULL);
        ConvertBlobToDetectionResults(blob, frames, detection_result);
        gst_structure_free(detection_result);
    }
}
} // namespace

PostProcFunction EXTRACT_DETECTION_RESULTS = ExtractDetectionResults;
