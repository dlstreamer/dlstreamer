/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "meta_attacher.hpp"

#include "gva_base_inference.h"
#include "gva_tensor_meta.h"
#include "gva_utils.h"
#include "processor_types.h"

#include "inference_backend/safe_arithmetic.h"

#include <exception>
#include <memory>

using namespace PostProcessing;

namespace {

/**
 * Compares to metas of type GstVideoRegionOfInterestMeta by roi_type and coordinates.
 *
 * @param[in] left - pointer to first GstVideoRegionOfInterestMeta operand.
 * @param[in] right - pointer to second GstVideoRegionOfInterestMeta operand.
 *
 * @return true if given metas are equal, false otherwise.
 */
inline bool sameRegion(const GstVideoRegionOfInterestMeta *left, const GstVideoRegionOfInterestMeta *right) {
    return left->roi_type == right->roi_type && left->x == right->x && left->y == right->y && left->w == right->w &&
           left->h == right->h;
}

} // namespace

ModelImageInputInfo MetaAttacher::input_info = {};

MetaAttacher::Ptr MetaAttacher::create(const ModelImageInputInfo &input_image_info) {
    MetaAttacher::input_info = input_image_info;

    return MetaAttacher::Ptr(new TensorToFrameAttacher());
}

void TensorToFrameAttacher::attach(const MetasTable &metas, GstBuffer *buffer) {

    for (size_t i = 0; i < 1; ++i) {

        for (GstStructure *tensor_data : metas[i]) {
            gva_buffer_check_and_make_writable(&buffer, __PRETTY_FUNCTION__);
            GstGVATensorMeta *tensor = GST_GVA_TENSOR_META_ADD(buffer);
            if (tensor->data) {
                gst_structure_free(tensor->data);
            }
            tensor->data = tensor_data;
            gst_structure_set(tensor->data, "element_id", G_TYPE_STRING, NULL, NULL);
        }
    }
}
