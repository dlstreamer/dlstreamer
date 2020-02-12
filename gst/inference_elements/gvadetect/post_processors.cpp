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
#include "post_processors.h"
#include "post_processors_util.h"
#include "video_frame.h"

#define UNUSED(x) (void)(x)

namespace {

using namespace InferenceBackend;

gboolean TensorToBBoxSSD(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                         std::vector<InferenceFrame> frames, GstStructure *detection_result) {
    ITT_TASK(__FUNCTION__);
    // Read labels and roi_scale from GstStructure config
    GValueArray *labels = nullptr;
    gdouble roi_scale = 1.0;
    gst_structure_get_array(detection_result, "labels", &labels);
    gst_structure_get_double(detection_result, "roi_scale", &roi_scale);

    // Check whether we can handle this blob instead iterator
    for (const auto &blob_iter : output_blobs) {
        InferenceBackend::OutputBlob::Ptr blob = blob_iter.second;
        if (!blob) {
            GST_ERROR("Blob pointer is null");
            return false;
        }
        const float *data = (const float *)blob->GetData();
        if (data == nullptr) {
            GST_ERROR("Blob data pointer is null");
            return false;
        }
        auto dims = blob->GetDims();
        guint dims_size = dims.size();

        static const guint min_dims_size = 2;
        if (dims_size < min_dims_size) {
            GST_ERROR(
                "Output blob converter by default: Output blob with inference results has %d dimensions, but it should "
                "have at least %d. Boxes won't be extracted\n",
                dims_size, min_dims_size);
            return false;
        }
        for (guint i = min_dims_size + 1; i < dims_size; ++i) {
            if (dims[dims_size - i] != 1) {
                GST_ERROR(
                    "Output blob converter by default: All output blob dimensions, except object size and max objects "
                    "count, must be equal to 1. Boxes won't be extracted\n");
                return false;
            }
        }

        guint object_size = dims[dims_size - 1];
        static const guint supported_object_size = 7;
        if (object_size != supported_object_size) { // SSD DetectionOutput format
            GST_ERROR("Output blob converter by default: Object size dimension of output blob is set to %d and "
                      "doesn't "
                      "equal to "
                      "supported - %d. Boxes won't be extracted\n",
                      object_size, supported_object_size);
            return false;
        }

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
            if (confidence < gva_detect->threshold) {
                continue;
            }

            // This post processing happens not in main gstreamer thread (but in separate one). Thus, we can run into a
            // problem where buffer's gva_base_inference is already cleaned up (all frames are pushed from decoder), so
            // we can't use gva_base_inference's GstVideoInfo to get width and height. In this case we use w and h of
            // InferenceFrame to make VideoFrame box adding logic work correctly
            if (frames[image_id].gva_base_inference->info) {
                video_info = *frames[image_id].gva_base_inference->info;
            } else {
                video_info.width = frames[image_id].roi.w;
                video_info.height = frames[image_id].roi.h;
            }
            GVA::VideoFrame video_frame(frames[image_id].buffer, frames[image_id].info);

            // TODO: ckeck if we can simplify below code further
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

            gint ix_min = (gint)(x_min * video_info.width + 0.5);
            gint iy_min = (gint)(y_min * video_info.height + 0.5);
            gint ix_max = (gint)(x_max * video_info.width + 0.5);
            gint iy_max = (gint)(y_max * video_info.height + 0.5);

            video_frame.add_region(ix_min, iy_min, ix_max - ix_min, iy_max - iy_min, label_id, confidence,
                                   gst_structure_copy(detection_result)); // each ROI gets its own copy, which is then
                                                                          // owned by GstVideoRegionOfInterestMeta
        }
    }
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    if (labels)
        g_value_array_free(labels);
    G_GNUC_END_IGNORE_DEPRECATIONS
    return true;
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

    GValueArray *labels = nullptr;
    gst_structure_get_array(detection_result, "labels", &labels);

    GVA::VideoFrame video_frame(frame.buffer, frame.info);

    for (DetectedObject &object : objects) {
        video_frame.add_region(object.x, object.y, object.w, object.h, object.class_id, object.confidence,
                               gst_structure_copy(detection_result)); // each ROI gets its own copy, which is then
                                                                      // owned by GstVideoRegionOfInterestMeta
    }

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    if (labels)
        g_value_array_free(labels);
    G_GNUC_END_IGNORE_DEPRECATIONS
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

struct RawNetOutInfo {
    const float *blob_data;
    Dequantizer dequantizer;
};

void fillRawNetOutMoviTL(RawNetOutInfo &info, const size_t anchor_index, const size_t cell_index, const float threshold,
                         float *converted_blob_data) {
    ITT_TASK(__FUNCTION__);
    if (info.blob_data == nullptr || converted_blob_data == nullptr) {
        GST_ERROR("Empty blob_data or converted_blob_data is not allowed");
        return;
    }

    static const size_t strides4D[] = {13 * 128, 128, 25, 1};
    const size_t offset = cell_index * strides4D[1] + anchor_index * strides4D[2];
    for (size_t i = 0; i < INDEX_COUNT; i++) {
        const size_t index = offset + i * strides4D[3];
        converted_blob_data[i] =
            info.dequantizer.dequantize((reinterpret_cast<const uint8_t *>(info.blob_data))[index]);
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

void fillRawNetOut(RawNetOutInfo &info, const size_t anchor_index, const size_t cell_index, const float threshold,
                   float *converted_blob_data) {
    ITT_TASK(__FUNCTION__);
    constexpr size_t K_OUT_BLOB_ITEM_N = 25;
    constexpr size_t K_2_DEPTHS = K_ANCHOR_SN * K_ANCHOR_SN;
    constexpr size_t K_3_DEPTHS = K_2_DEPTHS * K_OUT_BLOB_ITEM_N;

    const size_t common_offset = anchor_index * K_3_DEPTHS + cell_index;

    converted_blob_data[INDEX_X] = info.blob_data[common_offset + 1 * K_2_DEPTHS]; // x
    converted_blob_data[INDEX_Y] = info.blob_data[common_offset + 0 * K_2_DEPTHS]; // y
    converted_blob_data[INDEX_W] = info.blob_data[common_offset + 3 * K_2_DEPTHS]; // w
    converted_blob_data[INDEX_H] = info.blob_data[common_offset + 2 * K_2_DEPTHS]; // h

    converted_blob_data[INDEX_SCALE] = info.blob_data[common_offset + 4 * K_2_DEPTHS]; // scale

    for (size_t i = INDEX_CLASS_PROB_BEGIN; i < INDEX_CLASS_PROB_END; ++i) {
        converted_blob_data[i] =
            info.blob_data[common_offset + i * K_2_DEPTHS] * converted_blob_data[INDEX_SCALE]; // scaled probs
        if (converted_blob_data[i] <= threshold)
            converted_blob_data[i] = 0.f;
    }
}

using RawNetOutExtractor = std::function<void(RawNetOutInfo &, const size_t, const size_t, const float, float *)>;

bool TensorToBBoxYoloV2TinyCommon(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                                  std::vector<InferenceFrame> frames, GstStructure *detection_result,
                                  RawNetOutExtractor extractor) {
    ITT_TASK(__FUNCTION__);
    if (frames.size() != 1) {
        const gchar *converter = gst_structure_get_string(detection_result, "converter");
        GST_ERROR("Batch size not equal to 1 is not supported for this post proc converter: '%s', boxes won't be "
                  "extracted\n",
                  converter);
        return false;
    }
    GstGvaDetect *gva_detect = (GstGvaDetect *)frames[0].gva_base_inference;

    static const float K_ANCHOR_SCALES[] = {1.08f, 1.19f, 3.42f, 4.41f, 6.63f, 11.38f, 9.42f, 5.11f, 16.62f, 10.52f};
    std::vector<DetectedObject> objects;
    for (const auto &blob_iter : output_blobs) {
        InferenceBackend::OutputBlob::Ptr blob = blob_iter.second;
        if (!blob) {
            GST_ERROR("Blob pointer is null");
            return false;
        }
        RawNetOutInfo info = {
            (const float *)blob->GetData(), // blob_data
            Dequantizer(detection_result)   // dequantizer
        };

        if (info.blob_data == nullptr) {
            GST_ERROR("Blob data pointer is null");
            return false;
        }

        float raw_netout[INDEX_COUNT];
        for (size_t k = 0; k < 5; k++) {
            float anchor_w = K_ANCHOR_SCALES[k * 2];
            float anchor_h = K_ANCHOR_SCALES[k * 2 + 1];

            for (size_t i = 0; i < K_ANCHOR_SN; i++) {
                for (size_t j = 0; j < K_ANCHOR_SN; j++) {
                    extractor(info, k, i * K_ANCHOR_SN + j, gva_detect->threshold, raw_netout);

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
    }
    storeObjects(objects, frames[0], detection_result);
    return true;
}

int v3EntryIndex(int side, int lcoords, int lclasses, int location, int entry) {
    int n = location / (side * side);
    int loc = location % (side * side);
    return n * side * side * (lcoords + lclasses + 1) + entry * side * side + loc;
}

} // namespace Yolo

bool TensorToBBoxYoloV3(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                        std::vector<InferenceFrame> frames, GstStructure *detection_result) {
    ITT_TASK(__FUNCTION__);
    if (frames.size() != 1) {
        const gchar *converter = gst_structure_get_string(detection_result, "converter");
        GST_ERROR("Batch size not equal to 1 is not supported for this post proc converter: '%s', boxes won't be "
                  "extracted\n",
                  converter);
        return false;
    }
    GstGvaDetect *gva_detect = (GstGvaDetect *)frames[0].gva_base_inference;
    std::vector<Yolo::DetectedObject> objects;

    const int coords = 4;
    const int num = 3;
    const int classes = 80;
    const float input_size = 416;
    const float anchors[] = {10.0f, 13.0f, 16.0f,  30.0f,  33.0f, 23.0f,  30.0f,  61.0f,  62.0f,
                             45.0f, 59.0f, 119.0f, 116.0f, 90.0f, 156.0f, 198.0f, 373.0f, 326.0f};
    for (const auto &blob_iter : output_blobs) {
        InferenceBackend::OutputBlob::Ptr blob = blob_iter.second;
        if (!blob) {
            GST_ERROR("Blob pointer is null");
            return false;
        }
        auto dims = blob->GetDims();
        if (dims.size() != 4 || dims[2] != dims[3]) {
            throw std::runtime_error("YoloV3: Invalid tensor dimensions");
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
            throw std::runtime_error("YoloV3: Invalid output size");
        }

        const float *output_blob = (const float *)blob->GetData();
        if (output_blob == nullptr) {
            GST_ERROR("YoloV3: Blob data pointer is null");
            return false;
        }

        const int side_square = side * side;
        for (int i = 0; i < side_square; ++i) {
            const int row = i / side;
            const int col = i % side;
            for (int n = 0; n < num; ++n) {

                const int obj_index = Yolo::v3EntryIndex(side, coords, classes, n * side_square + i, coords);
                const int box_index = Yolo::v3EntryIndex(side, coords, classes, n * side_square + i, 0);

                const float scale = output_blob[obj_index];
                if (scale < gva_detect->threshold)
                    continue;
                const float x = (col + output_blob[box_index + 0 * side_square]) / side * input_size;
                const float y = (row + output_blob[box_index + 1 * side_square]) / side * input_size;

                const float width = std::exp(output_blob[box_index + 2 * side_square]) * anchors[anchor_offset + 2 * n];
                const float height =
                    std::exp(output_blob[box_index + 3 * side_square]) * anchors[anchor_offset + 2 * n + 1];

                for (int j = 0; j < classes; ++j) {
                    const int class_index =
                        Yolo::v3EntryIndex(side, coords, classes, n * side_square + i, coords + 1 + j);
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
}

bool TensorToBBoxYoloV2Tiny(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                            std::vector<InferenceFrame> frames, GstStructure *detection_result) {
    return Yolo::TensorToBBoxYoloV2TinyCommon(output_blobs, frames, detection_result, Yolo::fillRawNetOut);
}

bool TensorToBBoxYoloV2TinyMoviTL(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                                  std::vector<InferenceFrame> frames, GstStructure *detection_result) {
    return Yolo::TensorToBBoxYoloV2TinyCommon(output_blobs, frames, detection_result, Yolo::fillRawNetOutMoviTL);
}

bool ConvertBlobToDetectionResults(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                                   std::vector<InferenceFrame> frames, GstStructure *detection_result) {
    ITT_TASK(__FUNCTION__);
    if (output_blobs.empty())
        std::logic_error("Map with layer_name and blob is empty.");
    if (detection_result == nullptr)
        throw std::runtime_error("Post proc layer is null during post processing. Cannot access null object.");

    std::string converter = "";
    if (gst_structure_has_field(detection_result, "converter")) {
        converter = (std::string)gst_structure_get_string(detection_result, "converter");
    } else {
        converter = "tensor_to_bbox_ssd";
        gst_structure_set(detection_result, "converter", G_TYPE_STRING, converter.c_str(), NULL);
    }
    static std::map<std::string, std::function<bool(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &,
                                                    std::vector<InferenceFrame>, GstStructure *)>>
        do_conversion{{"tensor_to_bbox_ssd", TensorToBBoxSSD}, // default post processing
                      {"DetectionOutput", TensorToBBoxSSD},    // GVA plugin R1.2 backward compatibility
                      {"tensor_to_bbox_yolo_v2_tiny", TensorToBBoxYoloV2Tiny},
                      {"tensor_to_bbox_yolo_v2_tiny_moviTL", TensorToBBoxYoloV2TinyMoviTL},
                      {"tensor_to_bbox_yolo_v3", TensorToBBoxYoloV3}};
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
    return do_conversion[converter](output_blobs, frames, detection_result);
} // namespace

void ExtractDetectionResults(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                             std::vector<InferenceFrame> frames,
                             const std::map<std::string, GstStructure *> &model_proc, const gchar *model_name) {
    ITT_TASK(__FUNCTION__);
    std::string layer_name = "";
    GstStructure *detection_result = nullptr;
    if (model_proc.empty()) {
        layer_name = output_blobs.cbegin()->first;
        detection_result = gst_structure_new_empty("detection");
    } else {
        // TODO: To delete this loop, it is necessary to pass to the function not the entire model_proc, which contains
        //       both the preprocessing and postprocessing information, but separately the postprocessing information.
        for (const auto &layer_out : output_blobs) {
            if (model_proc.find(layer_out.first) != model_proc.cend()) {
                layer_name = layer_out.first;
                detection_result = gst_structure_copy(model_proc.at(layer_name));
                gst_structure_set_name(detection_result, "detection");
            }
        }
        if (!detection_result) {
            layer_name = output_blobs.cbegin()->first;
            detection_result = gst_structure_new_empty("detection");
        }
    }
    gst_structure_set(detection_result, "layer_name", G_TYPE_STRING, layer_name.c_str(), "model_name", G_TYPE_STRING,
                      model_name, NULL);

    ConvertBlobToDetectionResults(output_blobs, frames, detection_result);
    gst_structure_free(detection_result); // free initial structure filled with information, common for each ROI
}
} // namespace

PostProcFunction EXTRACT_DETECTION_RESULTS = ExtractDetectionResults;