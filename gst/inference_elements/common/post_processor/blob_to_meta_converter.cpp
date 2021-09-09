/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "blob_to_meta_converter.h"

#include "converters/to_roi/ssd.h"
#include "converters/to_roi/yolo_base.h"
#include "converters/to_tensor/raw_data_copy.h"
#include "converters/to_tensor/to_keypoints_3d.h"
#include "converters/to_tensor/to_keypoints_hrnet.h"
#include "converters/to_tensor/to_keypoints_openpose.h"
#include "converters/to_tensor/to_label.h"
#include "converters/to_tensor/to_text.h"
#include "copy_blob_to_gststruct.h"

#include "gva_base_inference.h"

#include <gst/gst.h>

#include <algorithm>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace post_processing;

namespace {
std::string getConverterType(GstStructure *s) {
    if (s == nullptr || !gst_structure_has_field(s, "converter"))
        throw std::runtime_error("Couldn't determine converter type.");

    std::string converter_type = gst_structure_get_string(s, "converter");
    if (converter_type.empty())
        throw std::runtime_error("model_proc's output_processor has empty converter.");

    return converter_type;
}

std::string getDefaultTensorName(int inference_type, std::string layer_name) {
    // GstStructure name string does not support '\'
    std::replace(layer_name.begin(), layer_name.end(), '\\', ':');

    switch (inference_type) {
    case GST_GVA_DETECT_TYPE:
        return "detection";

    case GST_GVA_INFERENCE_TYPE:
        return "inference_layer_name:" + layer_name;

    case GST_GVA_CLASSIFY_TYPE:
        return "classification_layer_name:" + layer_name;

    default:
        throw std::runtime_error("Invalid inference_type.");
    }
}
void updateTensorNameIfNeeded(GstStructure *s, const std::string &default_name) {
    if (s == nullptr)
        throw std::runtime_error("GstStructure is null.");

    if (gst_structure_has_field(s, "attribute_name")) {
        const char *result_name = gst_structure_get_string(s, "attribute_name");
        gst_structure_set_name(s, result_name);
        return;
    }

    const std::string current_name = gst_structure_get_name(s);
    if (current_name == default_name)
        return;

    gst_structure_set_name(s, default_name.c_str());
}

size_t getKeypointsNumber(GstStructure *s) {
    if (s == nullptr || !gst_structure_has_field(s, "point_names"))
        throw std::runtime_error("\"point_names\" is not defined in model-proc file.");

    GValueArray *point_names = nullptr;
    gst_structure_get_array(s, "point_names", &point_names);
    if (!point_names)
        throw std::runtime_error("\"point_names\" is not defined in model-proc file.");

    size_t kepoints_number = point_names->n_values;

    g_value_array_free(point_names);

    return kepoints_number;
}
} // namespace

BlobToMetaConverter::BlobToMetaConverter(const std::string &model_name, const ModelImageInputInfo &input_image_info,
                                         GstStructureUniquePtr model_proc_output_info,
                                         const std::vector<std::string> &labels)
    : model_name(model_name), input_image_info(input_image_info),
      model_proc_output_info(std::move(model_proc_output_info)), labels(labels) {
}

BlobToMetaConverter::Ptr BlobToMetaConverter::create(GstStructure *model_proc_output_info, int inference_type,
                                                     const ModelImageInputInfo &input_image_info,
                                                     const std::string &model_name,
                                                     const std::vector<std::string> &labels,
                                                     const std::string &displayed_layer_name_in_meta) {
    std::string converter_name = getConverterType(model_proc_output_info);

    const std::string default_name = getDefaultTensorName(inference_type, displayed_layer_name_in_meta);
    GstStructureUniquePtr tensor =
        GstStructureUniquePtr(gst_structure_new_empty(default_name.c_str()), gst_structure_free);

    if (model_proc_output_info != nullptr)
        tensor.reset(gst_structure_copy(model_proc_output_info));

    updateTensorNameIfNeeded(tensor.get(), default_name);

    gst_structure_set(tensor.get(), "layer_name", G_TYPE_STRING, displayed_layer_name_in_meta.c_str(), "model_name",
                      G_TYPE_STRING, model_name.c_str(), NULL);

    switch (inference_type) {
    case GST_GVA_DETECT_TYPE: {
        return BlobToROIConverter::create(model_name, input_image_info, std::move(tensor), labels, converter_name);
    } break;

    case GST_GVA_INFERENCE_TYPE: {
        if (converter_name == RawDataCopyConverter::getName())
            return BlobToMetaConverter::Ptr(
                new RawDataCopyConverter(model_name, input_image_info, std::move(tensor), labels));
        else
            goto NOT_IMPLEMENTD;
    } break;

    case GST_GVA_CLASSIFY_TYPE: {
        if (converter_name == RawDataCopyConverter::getName())
            return BlobToMetaConverter::Ptr(
                new RawDataCopyConverter(model_name, input_image_info, std::move(tensor), labels));
        else if (converter_name == ToLabelConverter::getName())
            return BlobToMetaConverter::Ptr(
                new ToLabelConverter(model_name, input_image_info, std::move(tensor), labels));
        else if (converter_name == ToTextConverter::getName())
            return BlobToMetaConverter::Ptr(
                new ToTextConverter(model_name, input_image_info, std::move(tensor), labels));
        else if (converter_name == ToKeypointsHRnetConverter::getName())
            return BlobToMetaConverter::Ptr(
                new ToKeypointsHRnetConverter(model_name, input_image_info, std::move(tensor), labels));
        else if (converter_name == ToKeypoints3DConverter::getName())
            return BlobToMetaConverter::Ptr(
                new ToKeypoints3DConverter(model_name, input_image_info, std::move(tensor), labels));
        else if (converter_name == ToKeypointsOpenPoseConverter::getName())
            return BlobToMetaConverter::Ptr(new ToKeypointsOpenPoseConverter(
                model_name, input_image_info, std::move(tensor), labels, getKeypointsNumber(model_proc_output_info)));
        else
            goto NOT_IMPLEMENTD;
    } break;

    default:
        throw std::runtime_error("Invalid inference_type.");
    }

    return nullptr;

NOT_IMPLEMENTD:
    throw std::runtime_error("Converter \"" + converter_name + "\" is not implemented.");
}

GVA::Tensor BlobToTensorConverter::createTensor() const {
    GstStructure *tensor_data = nullptr;
    if (getModelProcOutputInfo()) {
        tensor_data = copy(getModelProcOutputInfo().get(), gst_structure_copy);
    } else {
        throw std::runtime_error("Failed to initialize classification result structure: model-proc is null.");
    }
    if (!tensor_data) {
        throw std::runtime_error("Failed to initialize classification result tensor.");
    }

    return GVA::Tensor(tensor_data);
}

const std::string BlobToTensorConverter::RawTensorCopyingToggle::id = "disable-tensor-copying";
const std::string BlobToTensorConverter::RawTensorCopyingToggle::deprecation_message =
    "In pipelines with gvaclassify, in addition to classification results, a raw inference tensor is added to the "
    "metadata. This functionality will be removed in future releases. Set environment variable "
    "ENABLE_GVA_FEATURES=disable-tensor-copying to disable copying to "
    "frame metadata of raw tensor after inference.";
