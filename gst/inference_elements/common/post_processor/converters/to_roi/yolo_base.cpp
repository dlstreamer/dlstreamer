/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "yolo_base.h"
#include "post_processor/blob_to_meta_converter.h"
#include "yolo_v2.h"
#include "yolo_v3.h"

#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"

#include <gst/gst.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace post_processing;

namespace {
std::vector<float> getAnchors(GstStructure *s) {
    if (!gst_structure_has_field(s, "anchors"))
        throw std::runtime_error("model proc does not have \"anchors\" parameter.");
    GValueArray *arr = nullptr;
    gst_structure_get_array(s, "anchors", &arr);
    std::vector<float> anchors;
    if (arr) {
        anchors.reserve(arr->n_values);
        for (guint i = 0; i < arr->n_values; ++i)
            anchors.push_back(g_value_get_double(g_value_array_get_nth(arr, i)));
        g_value_array_free(arr);
    } else {
        throw std::runtime_error("\"anchors\" array is null.");
    }
    return anchors;
}

std::map<size_t, std::vector<size_t>> getMask(GstStructure *s, size_t bbox_number_on_cell, size_t cells_number) {

    if (!gst_structure_has_field(s, "masks"))
        throw std::runtime_error("model proc does not have \"masks\" parameter.");
    GValueArray *arr = nullptr;
    gst_structure_get_array(s, "masks", &arr);
    std::vector<size_t> masks;
    if (arr) {
        masks.reserve(arr->n_values);
        for (guint i = 0; i < arr->n_values; ++i)
            masks.push_back(g_value_get_int(g_value_array_get_nth(arr, i)));
        g_value_array_free(arr);
    } else {
        throw std::runtime_error("\"masks\" array is null.");
    }
    std::map<size_t, std::vector<size_t>> mask;
    std::vector<size_t> one_side_mask;
    one_side_mask.reserve(bbox_number_on_cell);
    for (size_t i = 0; i < masks.size(); ++i) {
        if (i != 0 && i % bbox_number_on_cell == 0) {
            mask.insert({cells_number, one_side_mask});
            cells_number *= 2;
            one_side_mask.clear();
        }
        one_side_mask.push_back(masks[i]);
    }
    mask.insert({cells_number, one_side_mask});
    return mask;
}

size_t getClassesNum(GstStructure *s) {
    int classes = 0;
    if (gst_structure_has_field(s, "classes")) {
        gst_structure_get_int(s, "classes", &classes);
    } else {
        throw std::runtime_error("model proc does not have \"classes\" parameter.");
    }
    return classes;
}

size_t getCellsNumber(GstStructure *s) {
    int cells_number = 13;
    if (gst_structure_has_field(s, "cells_number")) {
        gst_structure_get_int(s, "cells_number", &cells_number);
    }

    return cells_number;
}

size_t getBboxNumberOnCell(GstStructure *s) {
    int bbox_number_on_cell = 0;
    if (gst_structure_has_field(s, "bbox_number_on_cell")) {
        gst_structure_get_int(s, "bbox_number_on_cell", &bbox_number_on_cell);
    } else {
        GST_WARNING("model proc does not have \"bbox_number_on_cell\" parameter.");
    }
    return bbox_number_on_cell;
}

double getIOUThreshold(GstStructure *s) {
    double iou_threshold = 0.5;
    if (gst_structure_has_field(s, "iou_threshold")) {
        gst_structure_get_double(s, "iou_threshold", &iou_threshold);
    }

    return iou_threshold;
}

bool getDoClsSoftmax(GstStructure *s) {
    gboolean do_cls_sftm = FALSE;
    if (gst_structure_has_field(s, "do_cls_softmax")) {
        gst_structure_get_boolean(s, "do_cls_softmax", &do_cls_sftm);
    }

    return do_cls_sftm;
}

bool getOutputSigmoidActivation(GstStructure *s) {
    gboolean do_coords_sgmd = FALSE;
    if (gst_structure_has_field(s, "output_sigmoid_activation")) {
        gst_structure_get_boolean(s, "output_sigmoid_activation", &do_coords_sgmd);
    }

    return do_coords_sgmd;
}

} // namespace

BlobToMetaConverter::Ptr YOLOBaseConverter::create(const std::string &model_name,
                                                   const ModelImageInputInfo &input_image_info,
                                                   GstStructureUniquePtr model_proc_output_info,
                                                   const std::vector<std::string> &labels,
                                                   const std::string &converter_name, double confidence_threshold) {
    const auto classes_number = getClassesNum(model_proc_output_info.get());
    const auto anchors = getAnchors(model_proc_output_info.get());
    const auto iou_threshold = getIOUThreshold(model_proc_output_info.get());
    const auto cells_number = getCellsNumber(model_proc_output_info.get());
    const auto do_cls_softmax = getDoClsSoftmax(model_proc_output_info.get());
    const auto output_sigmoid_activation = getOutputSigmoidActivation(model_proc_output_info.get());
    auto bbox_number_on_cell = getBboxNumberOnCell(model_proc_output_info.get());

    if (converter_name == YOLOv2Converter::getName()) {
        if (!bbox_number_on_cell)
            bbox_number_on_cell = 5;
        return BlobToMetaConverter::Ptr(
            new YOLOv2Converter(model_name, input_image_info, std::move(model_proc_output_info), labels,
                                confidence_threshold, classes_number, anchors, cells_number, cells_number,
                                iou_threshold, bbox_number_on_cell, do_cls_softmax, output_sigmoid_activation));
    }
    if (converter_name == YOLOv3Converter::getName()) {
        if (!bbox_number_on_cell)
            bbox_number_on_cell = 3;
        const auto masks = getMask(model_proc_output_info.get(), bbox_number_on_cell, cells_number);
        if (input_image_info.width / 32 != cells_number) {
            GST_WARNING("The size of the input layer of the model does not match the specified number of cells. Verify "
                        "your \"cells_number\" field in model_proc.");
        }
        return BlobToMetaConverter::Ptr(new YOLOv3Converter(
            model_name, input_image_info, std::move(model_proc_output_info), labels, confidence_threshold,
            classes_number, anchors, masks, cells_number, cells_number, iou_threshold, bbox_number_on_cell,
            input_image_info.width, do_cls_softmax, output_sigmoid_activation));
    }
    return nullptr;
}
