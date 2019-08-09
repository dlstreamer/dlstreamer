/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
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
#include "inference_impl.h"

#include "post_processors.h"

namespace {

using namespace InferenceBackend;

void ExtractInferenceResults(const std::map<std::string, OutputBlob::Ptr> &output_blobs,
                             std::vector<InferenceROI> frames,
                             const std::map<std::string, GstStructure *> & /*model_proc*/, const gchar *model_name) {
    int batch_size = frames.size();

    for (auto blob_iter : output_blobs) {
        const char *layer_name = blob_iter.first.c_str();
        OutputBlob::Ptr blob = blob_iter.second;
        if (blob == nullptr)
            throw std::runtime_error("Blob is empty. Cannot access to null object.");

        const uint8_t *data = (const uint8_t *)blob->GetData();
        auto dims = blob->GetDims();
        int size = GetUnbatchedSizeInBytes(blob, batch_size);

        for (int b = 0; b < batch_size; b++) {
            InferenceROI &frame = frames[b];

            // find or create new meta
            GstGVATensorMeta *meta =
                find_tensor_meta_ext(frame.buffer, model_name, layer_name, frame.gva_base_inference->inference_id);
            if (!meta) {
                meta = GST_GVA_TENSOR_META_ADD(frame.buffer);
                meta->precision = static_cast<GVAPrecision>((int)blob->GetPrecision());
                meta->layout = static_cast<GVALayout>((int)blob->GetLayout());
                meta->rank = dims.size();
                if (meta->rank > GVA_TENSOR_MAX_RANK)
                    meta->rank = GVA_TENSOR_MAX_RANK;
                for (guint i = 0; i < meta->rank; i++) {
                    meta->dims[i] = dims[i];
                }
                meta->layer_name = g_strdup(layer_name);
                meta->model_name = g_strdup(model_name);
                meta->element_id = frame.gva_base_inference->inference_id;
                meta->total_bytes = size * meta->dims[0];
                meta->data = g_slice_alloc0(meta->total_bytes);
            }
            memcpy(meta->data, data + b * size, size);
        }
    }
}

} // namespace

PostProcFunction EXTRACT_INFERENCE_RESULTS = ExtractInferenceResults;
