/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "blob_to_roi_converter.h"
#include "ov_default.h"
#include "yolo_base.h"

#include "inference_backend/logger.h"

#include <gst/gst.h>

#include <algorithm>
#include <cassert>
#include <exception>
#include <map>
#include <memory>
#include <string>

using namespace post_processing;

BlobToMetaConverter::Ptr BlobToROIConverter::create(const std::string &model_name,
                                                    const ModelImageInputInfo &input_image_info,
                                                    GstStructureUniquePtr model_proc_output_info,
                                                    const std::vector<std::string> &labels,
                                                    const std::string &converter_name) {
    if (model_proc_output_info == nullptr)
        throw std::runtime_error("model_proc_output_info have been hot initialized.");

    double confidence_threshold;
    if (not gst_structure_get_double(model_proc_output_info.get(), "confidence_threshold", &confidence_threshold))
        throw std::runtime_error("Have not been gotten confidence_threshold.");

    if (converter_name.empty() or converter_name == "tensor_to_bbox_ssd")
        return BlobToMetaConverter::Ptr(new OVDefaultConverter(
            model_name, input_image_info, std::move(model_proc_output_info), labels, confidence_threshold));
    else if (converter_name == "tensor_to_bbox_yolo_v2" or converter_name == "tensor_to_bbox_yolo_v3")
        return YOLOBaseConverter::create(model_name, input_image_info, std::move(model_proc_output_info), labels,
                                         converter_name, confidence_threshold);
    else
        throw std::runtime_error("Converter \"" + converter_name + "\" is not implemented.");

    return nullptr;
}

TensorsTable BlobToROIConverter::toTensorsTable(const DetectedObjectsTable &bboxes_table) const {
    size_t batch_size = getModelInputImageInfo().batch_size;

    if (bboxes_table.size() != batch_size)
        throw std::logic_error("bboxes_table and batch_size must be equal.");

    TensorsTable tensors_table(batch_size);

    for (size_t image_id = 0; image_id < batch_size; ++image_id) {
        auto &tensors_batch = tensors_table[image_id];
        const auto &bboxes = bboxes_table[image_id];
        for (const DetectedObject &object : bboxes) {
            tensors_batch.emplace_back(object.toTensor(getModelProcOutputInfo()));
        }
    }

    return tensors_table;
}

TensorsTable BlobToROIConverter::storeObjects(DetectedObjectsTable &objects_table) const {
    ITT_TASK(__FUNCTION__);
    if (need_nms)
        for (auto &objects : objects_table)
            runNms(objects);

    return toTensorsTable(objects_table);
}

void BlobToROIConverter::runNms(std::vector<DetectedObject> &candidates) const {
    ITT_TASK(__FUNCTION__);
    std::sort(candidates.rbegin(), candidates.rend());

    for (auto p_first_candidate = candidates.begin(); p_first_candidate != candidates.end(); ++p_first_candidate) {
        const auto &first_candidate = *p_first_candidate;
        double first_candidate_area = first_candidate.w * first_candidate.h;

        for (auto p_candidate = p_first_candidate + 1; p_candidate != candidates.end();) {
            const auto &candidate = *p_candidate;

            const double inter_width = std::min(first_candidate.x + first_candidate.w, candidate.x + candidate.w) -
                                       std::max(first_candidate.x, candidate.x);
            const double inter_height = std::min(first_candidate.y + first_candidate.h, candidate.y + candidate.h) -
                                        std::max(first_candidate.y, candidate.y);
            if (inter_width <= 0.0 || inter_height <= 0.0) {
                ++p_candidate;
                continue;
            }

            const double inter_area = inter_width * inter_height;
            const double candidate_area = candidate.w * candidate.h;
            const double union_area = candidate_area + first_candidate_area - inter_area;

            assert(union_area != 0 && "union_area is null. Both of the boxes have zero areas.");
            const double overlap = inter_area / union_area;
            if (overlap > iou_threshold)
                p_candidate = candidates.erase(p_candidate);
            else
                ++p_candidate;
        }
    }
}
