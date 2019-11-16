/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "classification_history.h"
#include "human_pose_estimator.h"
#include "gstgvaclassify.h"
#include "gva_base_inference.h"
#include "gva_roi_meta.h"
#include "gva_utils.h"
#include "post_processors_util.h"
#include "human_pose.h"
#include "peak.h"
#include "post_processors.h"

namespace {

using namespace InferenceBackend;
void copy_buffer_to_structure(GstStructure *structure, const void *buffer, int size) {
    GVariant *v = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, buffer, size, 1);
    gsize n_elem;
    gst_structure_set(structure, "data_buffer", G_TYPE_VARIANT, v, "data", G_TYPE_POINTER,
                      g_variant_get_fixed_array(v, &n_elem, 1), NULL);
}

static void find_max_element_index(const float *array, int len, int *index, float *value) {
    *index = 0;
    *value = array[0];
    for (int i = 1; i < len; i++) {
        if (array[i] > *value) {
            *index = i;
            *value = array[i];
        }
    }
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

bool TensorToLabel(GstStructure *s, const float *data, gsize nbytes) {
    const gchar *_method = gst_structure_get_string(s, "method");
    std::string method = _method ? _method : "";
    bool bMax = method == "max";
    bool bCompound = method == "compound";
    bool bIndex = method == "index";

    if (!bMax && !bCompound && !bIndex)
        bMax = true;

    GValueArray *labels = nullptr;
    if (!gst_structure_get_array(s, "labels", &labels))
        return false;
    if (!bIndex) {
        if (labels->n_values > (bCompound ? 2 : 1) * nbytes / sizeof(float)) {
            g_value_array_free(labels);
            return false;
        }
    }
    if (bMax) {
        int index;
        float confidence;
        find_max_element_index(data, labels->n_values, &index, &confidence);
        if (data[index] > 0) {
            const gchar *label = g_value_get_string(labels->values + index);
            gst_structure_set(s, "label", G_TYPE_STRING, label, "label_id", G_TYPE_INT, (gint)index, "confidence",
                              G_TYPE_DOUBLE, (gdouble)confidence, NULL);
        }
    } else if (bCompound) {
        std::string string;
        double threshold = 0.5;
        double confidence = 0;
        gst_structure_get_double(s, "threshold", &threshold);
        for (guint j = 0; j < (labels->n_values) / 2; j++) {
            const gchar *label = NULL;
            if (data[j] >= threshold) {
                label = g_value_get_string(labels->values + j * 2);
            } else if (data[j] > 0) {
                label = g_value_get_string(labels->values + j * 2 + 1);
            }
            if (label)
                string += label;
            if (data[j] >= confidence)
                confidence = data[j];
        }
        gst_structure_set(s, "label", G_TYPE_STRING, string.data(), "confidence", G_TYPE_DOUBLE, (gdouble)confidence,
                          NULL);
    } else if (bIndex) {
        std::string string;
        int max_value = 0;
        for (guint j = 0; j < nbytes / sizeof(float); j++) {
            int value = (int)data[j];
            if (value < 0 || (guint)value >= labels->n_values)
                break;
            if (value > max_value)
                max_value = value;
            string += g_value_get_string(labels->values + value);
        }
        if (max_value) {
            gst_structure_set(s, "label", G_TYPE_STRING, string.data(), NULL);
        }
    } else {
        double threshold = 0.5;
        double confidence = 0;
        gst_structure_get_double(s, "threshold", &threshold);
        for (guint j = 0; j < labels->n_values; j++) {
            if (data[j] >= threshold) {
                const gchar *label = g_value_get_string(labels->values + j);
                gst_structure_set(s, "label", G_TYPE_STRING, label, "confidence", G_TYPE_DOUBLE, (gdouble)confidence,
                                  NULL);
            }
            if (data[j] >= confidence)
                confidence = data[j];
        }
    }

    if (labels)
        g_value_array_free(labels);
    return true;
}

bool TensorToLabelMoviTL(GstStructure *s, const float *data, size_t size_in_bytes) {
    if (s == nullptr || data == nullptr || size_in_bytes == 0)
        return false;

    float *dequantized_data = new float[size_in_bytes];

    // scale
    auto original_data = reinterpret_cast<const uint8_t *>(data);
    for (size_t i = 0; i < size_in_bytes; i++) {
        dequantized_data[i] = dequantize(original_data[i]);
    }

    // softmax
    softMax(dequantized_data, size_in_bytes);

    // now it's back to probability
    bool result = TensorToLabel(s, dequantized_data, size_in_bytes * sizeof(float));
    delete[] dequantized_data;

    return result;
}

G_GNUC_END_IGNORE_DEPRECATIONS

bool TensorToText(GstStructure *s, const float *data, gsize nbytes) {
    gdouble scale = 1.0;
    gst_structure_get_double(s, "tensor_to_text_scale", &scale);
    gint precision = 2;
    gst_structure_get_int(s, "tensor_to_text_precision", &precision);
    std::stringstream stream;
    stream << std::fixed << std::setprecision(precision);
    for (size_t i = 0; i < nbytes / sizeof(float); i++) {
        if (i)
            stream << ", ";
        stream << data[i] * scale;
    }
    gst_structure_set(s, "label", G_TYPE_STRING, stream.str().data(), NULL);
    return true;
}

bool ConvertBlobToClassificationResults(GstStructure *s) {
    // get buffer and its size from s
    gsize size_in_bytes = 0;
    const float *data = (const float *)gva_get_tensor_data(s, &size_in_bytes);
    if (not data)
        return false;

    std::string converter =
        gst_structure_has_field(s, "converter") ? (std::string)gst_structure_get_string(s, "converter") : "";

    static std::map<std::string, std::function<bool(GstStructure *, const float *, gsize)>> do_conversion{
        {"tensor_to_label", TensorToLabel},
        {"attributes", TensorToLabel}, // GVA plugin R1.2 backward compatibility
        {"tensor_to_text", TensorToText},
        {"tensor2text", TensorToText},                  // GVA plugin R1.2 backward compatibility
        {"tensor_to_label_moviTL", TensorToLabelMoviTL} // for movi Tensorflow-lite based model
    };

    if (do_conversion.find(converter) == do_conversion.end()) { // Wrong converter set in model-proc file
        // if converter is set to empty string, we don't log it here: it was already logged previously one time
        if (converter == "")
            return false;

        std::string valid_converters = "";
        for (auto converter_iter = do_conversion.begin(); converter_iter != do_conversion.end(); ++converter_iter) {
            valid_converters += "\"" + converter_iter->first + "\"";
            if (std::next(converter_iter) != do_conversion.end()) // last element
                valid_converters += ", ";
        }
        // if converter is set to some non-empty string, we log error each frame
        GST_ERROR(
            "Unknown post proc converter: \"%s\". Please set \"converter\" field in model-proc file to one of the "
            "following values: %s",
            converter.c_str(), valid_converters.c_str());
        return false;
    }

    return do_conversion[converter](s, data, size_in_bytes);
}

void convertPoses2Array(const std::vector<HumanPose> &poses, float *data) {
    size_t i = 0;
    for (const auto &pose : poses){
        for (const auto &keypoint : pose.keypoints){
            data[i++] = keypoint.x;
            data[i++] = keypoint.y;
        }
    }
}
void ExtractClassificationResults(const std::map<std::string, OutputBlob::Ptr> &output_blobs,
                                  std::vector<InferenceROI> frames,
                                  const std::map<std::string, GstStructure *> &model_proc, const gchar *model_name) {
    int batch_size = frames.size();
    if(strstr(model_name, "human-pose") != NULL) {
            // find meta

        auto roi = &frames[0].roi;
        
        auto outputblob = output_blobs.begin();

        auto pafsblob = outputblob->second;
        auto heatmapblob = (++outputblob)->second;
        size_t pafsblob_size = pafsblob->GetSize();
        // g_print("\npafsblob_size: %lu", pafsblob_size);
        size_t heatmapblob_size = heatmapblob->GetSize();
        // g_print("\nheatmapblob_size: %lu", heatmapblob_size);
        const float* heatMapsData = reinterpret_cast<const float *>(heatmapblob->GetData());
        for (size_t i = 0; i < heatmapblob_size; ++i) {
            g_print("%f\n", heatMapsData[i]);
        }
        g_print("\n\n");
        const float* pafsData = reinterpret_cast<const float *>(pafsblob->GetData());
        // for (size_t i = 0; i < pafsblob_size; ++i) {
        //     g_print("%f ", pafsData[i]);
        // }
        // g_print("\n");
        

        const int heatMapOffset = heatmapblob->GetDims()[2] * heatmapblob->GetDims()[3];
        constexpr const int nHeatMaps = 18;
        const int pafOffset = pafsblob->GetDims()[2] * pafsblob->GetDims()[3];
        const int nPafs = pafsblob->GetDims()[1];
        const int featureMapWidth = heatmapblob->GetDims()[3];
        const int featureMapHeight = heatmapblob->GetDims()[2]; 
        const cv::Size imageSize(roi->w, roi->h);
        // g_print("\n\t%u, %u, %u, %u",roi->w, roi->h, roi->x, roi->y);
        GstGvaClassify *gva_classify = (GstGvaClassify *)frames[0].gva_base_inference;

        std::vector<HumanPose> poses = gva_classify->human_pose_estimator->postprocess(heatMapsData, heatMapOffset, nHeatMaps, pafsData, 
        pafOffset, nPafs, featureMapWidth, featureMapHeight, imageSize);
        size_t data_size = poses.size() * nHeatMaps * 2;
        float *data = new float[data_size];
        convertPoses2Array(poses, data);
        // float *data = poses.data();

        GstVideoRegionOfInterestMeta *meta = NULL;
        gpointer state = NULL;
        while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(frames[0].buffer, &state))) {
            if (meta->x == roi->x && meta->y == roi->y && meta->w == roi->w && meta->h == roi->h &&
                meta->id == roi->id) {
                break;
            }
        }
        if (!meta) {
            GST_DEBUG("Can't find ROI metadata");
        }

        // append new structure to ROI meta's params
        GstStructure *classification_result = gst_structure_new_empty("layer:custom_hpe_layer");
        gst_structure_set(classification_result, "layer_name", G_TYPE_STRING, "custom_hpe_layer", "model_name",
                            G_TYPE_STRING, model_name, "precision", G_TYPE_INT, (int)10, "layout",
                            G_TYPE_INT, (int)1, NULL);
        copy_buffer_to_structure(classification_result, data, data_size);
        gst_video_region_of_interest_meta_add_param(meta, classification_result);
        delete[] data;
    }
    else {
        for (const auto &blob_iter : output_blobs) {
            const std::string &layer_name = blob_iter.first;
            OutputBlob::Ptr blob = blob_iter.second;
            if (blob == nullptr)
                throw std::runtime_error("Blob is empty during post processing. Cannot access null object.");
            
            

            const uint8_t *data = (const uint8_t *)blob->GetData();
            int size = GetUnbatchedSizeInBytes(blob, batch_size);
            int rank = (int)blob->GetDims().size();

            for (int b = 0; b < batch_size; b++) {
                // find meta
                GstGvaClassify *gva_classify = (GstGvaClassify *)frames[b].gva_base_inference;
                auto roi = &frames[b].roi;
                GstVideoRegionOfInterestMeta *meta = NULL;
                gpointer state = NULL;
                while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(frames[b].buffer, &state))) {
                    if (meta->x == roi->x && meta->y == roi->y && meta->w == roi->w && meta->h == roi->h &&
                        meta->id == roi->id) {
                        break;
                    }
                }
                if (!meta) {
                    GST_DEBUG("Can't find ROI metadata");
                    continue;
                }

                // append new structure to ROI meta's params
                GstStructure *classification_result;
                const auto &post_proc = model_proc.find(layer_name);
                if (post_proc != model_proc.end()) {
                    classification_result = gst_structure_copy(post_proc->second);
                } else {
                    classification_result = gst_structure_new_empty(("layer:" + layer_name).data());
                }
                gst_structure_set(classification_result, "layer_name", G_TYPE_STRING, layer_name.data(), "model_name",
                                G_TYPE_STRING, model_name, "precision", G_TYPE_INT, (int)blob->GetPrecision(), "layout",
                                G_TYPE_INT, (int)blob->GetLayout(), "rank", G_TYPE_INT, rank, NULL);
                copy_buffer_to_structure(classification_result, data + b * size, size);
                if (post_proc != model_proc.end()) {
                    ConvertBlobToClassificationResults(classification_result);
                }

                gst_video_region_of_interest_meta_add_param(meta, classification_result);

                if (gva_classify->skip_classified_objects and meta->id > 0)
                    gva_classify->classification_history->UpdateROIParams(meta->id, classification_result);
            }
        }
    }
}
} // anonymous namespace

PostProcFunction EXTRACT_CLASSIFICATION_RESULTS = ExtractClassificationResults;
