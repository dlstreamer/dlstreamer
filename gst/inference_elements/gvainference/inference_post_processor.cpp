/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_post_processor.h"

#include "gva_tensor_meta.h"
#include "inference_backend/safe_arithmetic.h"
#include "inference_impl.h"

#include "copy_blob_to_gststruct.h"

#include <gst/gst.h>

#include <string>

using namespace InferenceBackend;
using namespace InferencePlugin;

InferencePostProcessor::InferencePostProcessor(const InferenceImpl *inference_impl) {
    if (inference_impl == nullptr)
        throw std::runtime_error("InferencePostProcessor could not be initialized with empty InferenceImple");
    auto &models = inference_impl->GetModels();
    if (models.size() == 0)
        return;
    if (models.size() > 1)
        throw std::runtime_error("Multimodels is not supported");
    model_name = models.front().name;
}

void InferencePostProcessor::process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                                     std::vector<std::shared_ptr<InferenceFrame>> &frames) {
    try {
        if (frames.empty())
            throw std::invalid_argument("There are no inference frames");

        size_t blob_id = 0;
        for (const auto &blob_iter : output_blobs) {
            OutputBlob::Ptr blob = blob_iter.second;
            if (not blob)
                throw std::invalid_argument("Output blob is empty");
            const char *layer_name = blob_iter.first.c_str();

            for (size_t b = 0; b < frames.size(); b++) {
                std::shared_ptr<InferenceFrame> frame = frames[b];

                GstGVATensorMeta *tensor_meta = GST_GVA_TENSOR_META_ADD(frame->buffer);
                if (not tensor_meta)
                    throw std::runtime_error("Failed to add GstGVATensorMeta instance");
                gst_structure_set_name(tensor_meta->data, layer_name);
                if (not gst_structure_has_name(tensor_meta->data, layer_name))
                    throw std::invalid_argument("Failed to set '" + std::string(layer_name) + "' as GstStructure name");

                CopyOutputBlobToGstStructure(blob, tensor_meta->data, model_name.c_str(), layer_name, frames.size(), b);

                // In different versions of GStreamer, metas are attached to the buffer in a different order. Thus, we
                // identify our meta using tensor_id.
                gst_structure_set(tensor_meta->data, "tensor_id", G_TYPE_INT, safe_convert<int>(blob_id), "element_id",
                                  G_TYPE_STRING, frame->gva_base_inference->model_instance_id, NULL);
            }
            ++blob_id;
        }
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to extract inference results"));
    }
}
