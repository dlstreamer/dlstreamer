/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <algorithm>
#include <cmath>
#include <functional>
#include <gst/gst.h>
#include <map>
#include <string>
#include <vector>

#include <opencv2/imgproc.hpp>

#include "gstgvainference.h"
#include "gva_base_inference.h"
#include "gva_tensor_meta.h"
#include "gva_utils.h"
#include "inference_backend/safe_arithmetic.h"
#include "inference_impl.h"
#include "video_frame.h"

#include "copy_blob_to_gststruct.h"
#include "post_processors.h"

namespace {

using namespace InferenceBackend;

void ExtractInferenceResults(const std::map<std::string, OutputBlob::Ptr> &output_blobs,
                             std::vector<InferenceFrame> frames,
                             const std::map<std::string, GstStructure *> & /*model_proc*/, const gchar *model_name) {
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
                InferenceFrame &frame = frames[b];

                GstGVATensorMeta *tensor_meta = GST_GVA_TENSOR_META_ADD(frame.buffer);
                if (not tensor_meta)
                    throw std::runtime_error("Failed to add GstGVATensorMeta instance");
                gst_structure_set_name(tensor_meta->data, layer_name);
                if (not gst_structure_has_name(tensor_meta->data, layer_name))
                    throw std::invalid_argument("Failed to set '" + std::string(layer_name) + "' as GstStructure name");

                CopyOutputBlobToGstStructure(blob, tensor_meta->data, model_name, layer_name, frames.size(), b);

                // In different versions of GStreamer, metas are attached to the buffer in a different order. Thus, we
                // identify our meta using tensor_id.
                gst_structure_set(tensor_meta->data, "tensor_id", G_TYPE_INT, safe_convert<int>(blob_id), "element_id",
                                  G_TYPE_STRING, frame.gva_base_inference->model_instance_id, NULL);
            }
            ++blob_id;
        }
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to extract inference results"));
    }
} // namespace

} // namespace

PostProcFunction EXTRACT_INFERENCE_RESULTS = ExtractInferenceResults;
