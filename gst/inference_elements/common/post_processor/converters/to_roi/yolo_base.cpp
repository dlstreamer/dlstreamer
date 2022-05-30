/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "yolo_base.h"
#include "yolo_v2.h"
#include "yolo_v3.h"

#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "safe_arithmetic.hpp"

#include <gst/gst.h>

#include <algorithm>
#include <map>
#include <memory>
#include <numeric>
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

size_t getClassesNum(GstStructure *s, size_t labels_num) {
    if (!gst_structure_has_field(s, "classes") && labels_num)
        return labels_num;

    int _classes = 0;
    gst_structure_get_int(s, "classes", &_classes);

    size_t classes = safe_convert<size_t>(_classes);

    if (!labels_num)
        return classes;

    if (classes < labels_num)
        GST_WARNING("Number of classes does not match with namber of labels: classes=%lu, labels=%lu.", classes,
                    labels_num);

    if (classes > labels_num)
        throw std::runtime_error("Number of classes greater then number of labels.");

    return classes;
}

std::pair<size_t, size_t> getCellsNumber(GstStructure *s) {
    const bool has_cells_number_x = gst_structure_has_field(s, "cells_number_x");
    const bool has_cells_number_y = gst_structure_has_field(s, "cells_number_y");
    const bool has_cells_number = gst_structure_has_field(s, "cells_number");

    if ((has_cells_number_x or has_cells_number_y) && has_cells_number)
        throw std::runtime_error("Please set only \"cells_number_x\" and \"cells_number_y\" or only \"cells_number\".");

    if ((has_cells_number_x and !has_cells_number_y) or (!has_cells_number_x and has_cells_number_y))
        throw std::runtime_error("Please set both \"cells_number_x\" and \"cells_number_y\" or only \"cells_number\".");

    if (has_cells_number_x && has_cells_number_y && !has_cells_number) {
        int cells_number_x = 0;
        int cells_number_y = 0;

        gst_structure_get_int(s, "cells_number_x", &cells_number_x);
        gst_structure_get_int(s, "cells_number_y", &cells_number_y);

        return std::make_pair(safe_convert<size_t>(cells_number_x), safe_convert<size_t>(cells_number_y));
    } else if (has_cells_number) {
        int cells_number = 0;

        gst_structure_get_int(s, "cells_number", &cells_number);

        return std::make_pair(safe_convert<size_t>(cells_number), safe_convert<size_t>(cells_number));
    } else {
        GST_WARNING(
            "model-proc does not have \"cells_number\" or \"cells_number_x\" and \"cells_number_y\" parameters.");
    }

    return std::make_pair(0, 0);
}

size_t getBboxNumberOnCell(GstStructure *s) {
    int bbox_number_on_cell = 0;
    if (gst_structure_has_field(s, "bbox_number_on_cell")) {
        gst_structure_get_int(s, "bbox_number_on_cell", &bbox_number_on_cell);
    } else {
        GST_WARNING("model-proc file does not have \"bbox_number_on_cell\" parameter.");
    }
    return safe_convert<size_t>(bbox_number_on_cell);
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

bool getDoTranspose(GstStructure *s) {
    gboolean do_transpose = FALSE;
    if (gst_structure_has_field(s, "do_transpose")) {
        gst_structure_get_boolean(s, "do_transpose", &do_transpose);
    }

    return do_transpose;
}

bool getOutputDoubleSigmoidActivation(GstStructure *s) {
    gboolean do_double_sigmoid = FALSE;
    if (gst_structure_has_field(s, "output_double_sigmoid_activation")) {
        gst_structure_get_boolean(s, "output_double_sigmoid_activation", &do_double_sigmoid);
    }

    return do_double_sigmoid;
}

} // namespace

size_t YOLOBaseConverter::tryAutomaticConfigWithDims(const std::vector<size_t> &dims, OutputDimsLayout layout,
                                                     size_t boxes, size_t classes, std::pair<size_t, size_t> &cells) {

    size_t &cells_x = cells.first;
    size_t &cells_y = cells.second;

    switch (layout) {
    case OutputDimsLayout::NBCxCy:
        cells_x = dims[2];
        cells_y = dims[3];
        break;
    case OutputDimsLayout::NCxCyB:
        cells_x = dims[1];
        cells_y = dims[2];
        break;
    case OutputDimsLayout::BCxCy:
        cells_x = dims[1];
        cells_y = dims[2];
        break;
    case OutputDimsLayout::CxCyB:
        cells_x = dims[0];
        cells_y = dims[1];
        break;

    default:
        throw std::runtime_error("Unsupported output layout.");
    }

    return cells.first * cells.second * boxes * (classes + 5);
}

std::pair<std::vector<size_t>, size_t> YOLOBaseConverter::getMinBlobDims(const ModelOutputsInfo &outputs_info) {
    auto min_size_dims = outputs_info.cbegin()->second;
    size_t min_blob_size = std::numeric_limits<size_t>::max();

    for (const auto &output_info_pair : outputs_info) {
        const auto &blob_dims = output_info_pair.second;

        size_t blob_size = std::accumulate(blob_dims.cbegin(), blob_dims.cend(), 1lu, std::multiplies<size_t>());
        min_blob_size = std::min(min_blob_size, blob_size);

        if (blob_size == min_blob_size)
            min_size_dims = blob_dims;
    }

    return std::pair<std::vector<size_t>, size_t>(min_size_dims, min_blob_size);
}

YOLOBaseConverter::OutputDimsLayout YOLOBaseConverter::getLayoutFromDims(const ModelOutputsInfo &outputs_info,
                                                                         const std::vector<float> &anchors,
                                                                         size_t classes) {
    auto min_blob_dims_pair = getMinBlobDims(outputs_info);
    const auto &min_blob_dims = min_blob_dims_pair.first;

    if (min_blob_dims.size() == 1)
        return YOLOBaseConverter::OutputDimsLayout::NO;

    size_t boxes = anchors.size() / (outputs_info.size() * 2);

    const auto find_it = std::find(min_blob_dims.begin(), min_blob_dims.end(), (boxes * (classes + 5)));

    if (find_it == min_blob_dims.cend())
        return YOLOBaseConverter::OutputDimsLayout::NO;

    size_t bbox_dim_i = std::distance(min_blob_dims.cbegin(), find_it);
    switch (min_blob_dims.size()) {
    case 3:
        switch (bbox_dim_i) {
        case 0:
            return YOLOBaseConverter::OutputDimsLayout::BCxCy;
        case 2:
            return YOLOBaseConverter::OutputDimsLayout::CxCyB;
        default:
            break;
        }
        break;
    case 4:
        switch (bbox_dim_i) {
        case 1:
            return YOLOBaseConverter::OutputDimsLayout::NBCxCy;
        case 3:
            return YOLOBaseConverter::OutputDimsLayout::NCxCyB;
        default:
            break;
        }
        break;
    default:
        break;
    }

    throw std::runtime_error("Unsupported output layout.");
}

bool YOLOBaseConverter::tryAutomaticConfig(const ModelImageInputInfo &input_info, const ModelOutputsInfo &outputs_info,
                                           OutputDimsLayout dims_layout, size_t classes,
                                           const std::vector<float> &anchors, std::pair<size_t, size_t> &cells,
                                           size_t &boxes) {

    boxes = anchors.size() / (outputs_info.size() * 2);
    cells.first = 0;
    cells.second = 0;

    auto min_blob_dims = getMinBlobDims(outputs_info);

    size_t batch_size = input_info.batch_size;

    if (dims_layout != OutputDimsLayout::NO) {
        size_t result_blob_size = tryAutomaticConfigWithDims(min_blob_dims.first, dims_layout, boxes, classes, cells);

        if (result_blob_size * batch_size == min_blob_dims.second)
            return true;
    }

    cells.first = input_info.width / default_downsample_degree;
    cells.second = input_info.height / default_downsample_degree;

    return min_blob_dims.second == batch_size * cells.first * cells.second * boxes * (classes + 5);
}

BlobToMetaConverter::Ptr YOLOBaseConverter::create(BlobToMetaConverter::Initializer initializer,
                                                   const std::string &converter_name, double confidence_threshold) {
    try {
        GstStructure *model_proc_output_info = initializer.model_proc_output_info.get();

        const auto classes_number = getClassesNum(model_proc_output_info, initializer.labels.size());
        if (!classes_number)
            throw std::runtime_error("Number of classes if null.");

        const auto anchors = getAnchors(model_proc_output_info);
        if (anchors.empty())
            throw std::runtime_error("Anchors is empty.");

        const auto iou_threshold = getIOUThreshold(model_proc_output_info);
        const auto do_cls_softmax = getDoClsSoftmax(model_proc_output_info);
        const auto output_sigmoid_activation = getOutputSigmoidActivation(model_proc_output_info);
        const auto do_transpose = getDoTranspose(model_proc_output_info);
        const auto do_double_sigmoid = getOutputDoubleSigmoidActivation(model_proc_output_info);

        std::pair<size_t, size_t> cells_number = getCellsNumber(model_proc_output_info);
        size_t bbox_number_on_cell = getBboxNumberOnCell(model_proc_output_info);

        OutputDimsLayout dims_layout = getLayoutFromDims(initializer.outputs_info, anchors, classes_number);

        if (!(cells_number.first && cells_number.second && bbox_number_on_cell)) {
            GST_WARNING("\"cells_number\" and \"bbox_number_on_cell\" weren't found in model-proc file. Trying to set "
                        "them automatically.");
            bool is_configurated =
                tryAutomaticConfig(initializer.input_image_info, initializer.outputs_info, dims_layout, classes_number,
                                   anchors, cells_number, bbox_number_on_cell);
            if (!is_configurated)
                throw std::runtime_error("Failed to match parameters. Please define them yourself in model-proc file.");

            GST_WARNING("Result of automatic configuration: cells_number_x=%lu, cells_number_y=%lu, "
                        "bbox_number_on_cell=%lu.",
                        cells_number.first, cells_number.second, bbox_number_on_cell);
        }

        const auto &outputs_info = initializer.outputs_info;

        if (anchors.size() != bbox_number_on_cell * 2 * outputs_info.size())
            throw std::runtime_error("Anchors size must be equal (bbox_number_on_cell * layers_number * 2).");

        OutputLayerShapeConfig output_shape_info(classes_number, cells_number.first, cells_number.second,
                                                 bbox_number_on_cell);
        YOLOBaseConverter::Initializer yolo_initializer = {anchors, output_shape_info, do_cls_softmax,
                                                           output_sigmoid_activation, do_transpose, do_double_sigmoid, dims_layout};

        if (converter_name == YOLOv2Converter::getName()) {
            YOLOv2Converter::checkModelProcOutputs(cells_number, bbox_number_on_cell, classes_number, outputs_info,
                                                   dims_layout, initializer.input_image_info);

            return BlobToMetaConverter::Ptr(
                new YOLOv2Converter(std::move(initializer), confidence_threshold, iou_threshold, yolo_initializer));
        }
        if (converter_name == YOLOv3Converter::getName()) {
            const auto masks =
                YOLOv3Converter::getMask(model_proc_output_info, bbox_number_on_cell,
                                         std::min(cells_number.first, cells_number.second), outputs_info.size());

            YOLOv3Converter::checkModelProcOutputs(cells_number, bbox_number_on_cell, classes_number, masks,
                                                   outputs_info, dims_layout, initializer.input_image_info);

            return BlobToMetaConverter::Ptr(new YOLOv3Converter(std::move(initializer), confidence_threshold,
                                                                iou_threshold, yolo_initializer, masks));
        }
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to create \"" + converter_name + "\" converter."));
    }
    return nullptr;
}

TensorsTable YOLOBaseConverter::convert(const OutputBlobs &output_blobs) const {
    ITT_TASK(__FUNCTION__);
    try {
        const auto &model_input_image_info = getModelInputImageInfo();
        size_t batch_size = model_input_image_info.batch_size;

        DetectedObjectsTable objects_table(batch_size);

        for (size_t batch_number = 0; batch_number < batch_size; ++batch_number) {
            auto &objects = objects_table[batch_number];

            for (const auto &blob_iter : output_blobs) {
                const InferenceBackend::OutputBlob::Ptr &blob = blob_iter.second;
                if (not blob)
                    throw std::invalid_argument("Output blob is nullptr.");

                size_t unbatched_size = blob->GetSize() / batch_size;
                parseOutputBlob(reinterpret_cast<const float *>(blob->GetData()) + unbatched_size * batch_number,
                                blob->GetDims(), unbatched_size, objects);
            }
        }

        return storeObjects(objects_table);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do YoloV3 post-processing."));
    }
    return TensorsTable{};
}
