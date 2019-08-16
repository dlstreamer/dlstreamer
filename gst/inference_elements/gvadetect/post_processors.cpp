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

namespace {

using namespace InferenceBackend;

gboolean TensorToBBoxSSD(const InferenceBackend::OutputBlob::Ptr &blob, std::vector<InferenceROI> frames,
                         GstStructure *layer_post_proc) {
    const float *data = (const float *)blob->GetData();
    if (data == nullptr) {
        GST_ERROR("Blob data pointer is null");
        return false;
    }
    auto dims = blob->GetDims();
    auto layout = blob->GetLayout();

    guint object_size = 0;
    guint max_proposal_count = 0;
    switch (layout) {
    case InferenceBackend::OutputBlob::Layout::NCHW:
        object_size = dims[3];
        max_proposal_count = dims[2];
        break;
    default:
        GST_ERROR("Only NCHW output layout for converter '%s' is supported, boxes won't be extracted\n",
                  gst_structure_get_string(layer_post_proc, "converter"));
        return false;
    }
    if (object_size != 7) { // SSD DetectionOutput format
        GST_ERROR("Only NCHW with W == 7 is supported, boxes won't be extracted\n");
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

struct DetectedObject {
    gint x;
    gint y;
    gint width;
    gint height;
    gfloat confidence;
    explicit DetectedObject(gfloat x, gfloat y, gfloat h, gfloat w, gfloat confidence, gfloat h_scale = 1.f,
                            gfloat w_scale = 1.f)
        : x(static_cast<gint>((x - w / 2) * w_scale)), y(static_cast<gint>((y - h / 2) * h_scale)),
          width(static_cast<gint>(w * w_scale)), height(static_cast<gint>(h * h_scale)), confidence(confidence) {
    }
    DetectedObject() = default;
    ~DetectedObject() = default;
    DetectedObject(const DetectedObject &) = default;
    DetectedObject(DetectedObject &&) = default;
    DetectedObject &operator=(const DetectedObject &) = default;
    DetectedObject &operator=(DetectedObject &&) = default;
    bool operator<(const DetectedObject &other) const {
        return this->confidence < other.confidence;
    }
};

std::vector<DetectedObject> run_nms(std::vector<DetectedObject> candidates, gdouble threshold) {
    std::vector<DetectedObject> nms_candidates;
    std::sort(candidates.begin(), candidates.end());

    while (candidates.size() > 0) {
        auto p_first_candidate = candidates.begin();
        const auto &first_candidate = *p_first_candidate;
        double first_candidate_area = first_candidate.width * first_candidate.height;

        for (auto p_candidate = p_first_candidate + 1; p_candidate != candidates.end();) {
            const auto &candidate = *p_candidate;

            gdouble inter_width = std::min(first_candidate.x + first_candidate.width, candidate.x + candidate.width) -
                                  std::max(first_candidate.x, candidate.x);
            gdouble inter_height =
                std::min(first_candidate.y + first_candidate.height, candidate.y + candidate.height) -
                std::max(first_candidate.y, candidate.y);
            if (inter_width <= 0.0 || inter_height <= 0.0) {
                ++p_candidate;
                continue;
            }

            gdouble inter_area = inter_width * inter_height;
            gdouble candidate_area = candidate.width * candidate.height;

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

bool TensorToBBoxYoloV2Tiny(const InferenceBackend::OutputBlob::Ptr &blob, std::vector<InferenceROI> frames,
                            GstStructure *layer_post_proc) {
    const gchar *converter = gst_structure_get_string(layer_post_proc, "converter");

    if (frames.size() != 1) {
        GST_ERROR(
            "Batch size not equal to 1 is not supported for this post proc converter: '%s', boxes won't be extracted\n",
            converter);
        return false;
    }
    gint image_width = frames[0].roi.w;
    gint image_height = frames[0].roi.h;
    GstGvaDetect *gva_detect = (GstGvaDetect *)frames[0].gva_base_inference;

    switch (blob->GetLayout()) {
    case InferenceBackend::OutputBlob::Layout::NC:
        break;
    default:
        GST_ERROR("Only NC output layout for converter '%s' is supported, boxes won't be extracted\n", converter);
        return false;
    }

    gint kAnchorSN = 13;
    gint kOutBlobItemN = 25;
    gint k2Depths = kAnchorSN * kAnchorSN;
    gint k3Depths = k2Depths * kOutBlobItemN;
    gfloat kAnchorScales[] = {1.08f, 1.19f, 3.42f, 4.41f, 6.63f, 11.38f, 9.42f, 5.11f, 16.62f, 10.52f};

    const gfloat *data = (const gfloat *)blob->GetData();
    if (data == nullptr) {
        GST_ERROR("Blob data pointer is null");
        return false;
    }

    std::vector<DetectedObject> objects;
    for (gint k = 0; k < 10; k += 2) {
        gfloat anchor_w = kAnchorScales[k];
        gfloat anchor_h = kAnchorScales[k + 1];

        for (gint i = 0; i < kAnchorSN; i++) {
            for (gint j = 0; j < kAnchorSN; j++) {
                gint x_pred_idx = (k >> 1) * k3Depths + 1 * k2Depths + i * kAnchorSN + j;
                gint y_pred_idx = (k >> 1) * k3Depths + 0 * k2Depths + i * kAnchorSN + j;
                gint w_pred_idx = (k >> 1) * k3Depths + 3 * k2Depths + i * kAnchorSN + j;
                gint h_pred_idx = (k >> 1) * k3Depths + 2 * k2Depths + i * kAnchorSN + j;
                gint scale_idx = (k >> 1) * k3Depths + 4 * k2Depths + i * kAnchorSN + j;

                gfloat x_pred = data[x_pred_idx];
                gfloat y_pred = data[y_pred_idx];
                gfloat w_pred = data[w_pred_idx];
                gfloat h_pred = data[h_pred_idx];
                gfloat scale = data[scale_idx];

                if (scale > 1.f) {
                    GST_WARNING_OBJECT(gva_detect, "scale weired %f", scale);
                }

                gfloat cx = (j + x_pred) * image_width / kAnchorSN;
                gfloat cy = (i + y_pred) * image_height / kAnchorSN;
                gfloat w = std::exp(w_pred) * anchor_w * image_width / kAnchorSN;
                gfloat h = std::exp(h_pred) * anchor_h * image_height / kAnchorSN;

                std::pair<gint, gfloat> max_info = std::make_pair(0, 0.f);

                for (gint l = 0; l < 20; l++) {
                    gint class_idx = (k >> 1) * k3Depths + (l + 5) * k2Depths + i * kAnchorSN + j;
                    gfloat class_prob = data[class_idx] * scale;
                    if (class_prob > 1.f) {
                        GST_WARNING_OBJECT(gva_detect, "class_prob weired %f", class_prob);
                    }
                    if (class_prob > max_info.second) {
                        max_info.first = l;
                        max_info.second = class_prob;
                    }
                }

                if (max_info.second > gva_detect->threshold) {
                    DetectedObject object(cx, cy, h, w, max_info.second);
                    objects.push_back(object);
                }
            }
        }
    }

    double nms_threshold = 0.5;
    gst_structure_get_double(layer_post_proc, "nms_threshold", &nms_threshold);
    objects = run_nms(objects, nms_threshold);

    for (const DetectedObject &object : objects)
        gst_buffer_add_video_region_of_interest_meta(frames[0].buffer, "", (object.x >= 0) ? object.x : 0,
                                                     (object.y >= 0) ? object.y : 0, object.width, object.height);

    // TODO: add ROI meta params?

    return true;
}

bool ConvertBlobToDetectionResults(const InferenceBackend::OutputBlob::Ptr &blob, std::vector<InferenceROI> frames,
                                   GstStructure *layer_post_proc) {
    if (blob == nullptr)
        throw std::runtime_error("Blob is empty during post processing. Cannot access null object.");
    if (layer_post_proc == nullptr)
        throw std::runtime_error("Post proc layer is null during post processing. Cannot access null object.");

    std::string converter = gst_structure_has_field(layer_post_proc, "converter")
                                ? (std::string)gst_structure_get_string(layer_post_proc, "converter")
                                : "";

    static std::map<std::string, std::function<bool(const InferenceBackend::OutputBlob::Ptr &,
                                                    std::vector<InferenceROI>, GstStructure *)>>
        do_conversion{
            {"", TensorToBBoxSSD}, // default post processing
            {"tensor_to_bbox_ssd", TensorToBBoxSSD},
            {"DetectionOutput", TensorToBBoxSSD}, // GVA plugin R1.2 backward compatibility
            {"tensor_to_bbox_yolo_v2_tiny", TensorToBBoxYoloV2Tiny},
        };

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
