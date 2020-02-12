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
#include "inference_impl.h"
#include "video_frame.h"

#include "post_processors.h"

namespace {

using namespace InferenceBackend;

void ExtractInferenceResults(const std::map<std::string, OutputBlob::Ptr> &output_blobs,
                             std::vector<InferenceFrame> frames,
                             const std::map<std::string, GstStructure *> & /*model_proc*/, const gchar *model_name) {
    if (frames.empty())
        std::logic_error("Vector of frames is empty.");
    int batch_size = frames.begin()->gva_base_inference->batch_size;
    size_t blob_id = 0;

    for (const auto &blob_iter : output_blobs) {
        const char *layer_name = blob_iter.first.c_str();
        OutputBlob::Ptr blob = blob_iter.second;
        if (blob == nullptr)
            throw std::runtime_error("Blob is empty. Cannot access to null object.");

        const uint8_t *data = (const uint8_t *)blob->GetData();
        auto dims = blob->GetDims();
        int unbatched_blob_size = GetUnbatchedSizeInBytes(blob, batch_size);

        for (size_t b = 0; b < frames.size(); b++) {
            InferenceFrame &frame = frames[b];
            GVA::VideoFrame video_frame(frame.buffer, frame.info);
            GVA::Tensor tensor = video_frame.add_tensor();

            // TODO: maybe we need to define possible gst structure fields instead hardcoded strings?
            size_t dims_size = std::min(dims.size(), (size_t)GVA_TENSOR_MAX_RANK);
            GValueArray *arr = g_value_array_new(dims_size);
            // TODO: check NCHW case
            GValue gvalue = G_VALUE_INIT;
            g_value_init(&gvalue, G_TYPE_UINT);

            // first dimension is batch-size set to 1 (cause we unbatched it)
            g_value_set_uint(&gvalue, 1U);
            g_value_array_append(arr, &gvalue);
            for (guint i = 1; i < dims_size; ++i) {
                g_value_set_uint(&gvalue, dims[i]);
                g_value_array_append(arr, &gvalue);
            }

            tensor.set_array("dims", arr);
            tensor.set_int("rank", dims_size);
            tensor.set_int("precision", (int)blob->GetPrecision());
            tensor.set_int("layout", (int)blob->GetLayout());
            tensor.set_string("layer_name", layer_name);
            tensor.set_string("model_name", model_name);
            tensor.set_string("element_id", frame.gva_base_inference->inference_id);
            tensor.set_int("total_bytes", unbatched_blob_size);
            // In different versions of GStreamer, metas are attached to the buffer in a different order. Thus, we
            // identify our meta using tensor_id.
            tensor.set_int("tensor_id", blob_id);

            GVariant *v =
                g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, data + b * unbatched_blob_size, unbatched_blob_size, 1);
            gsize n_elem;
            gst_structure_set(tensor.gst_structure(), "data_buffer", G_TYPE_VARIANT, v, "data", G_TYPE_POINTER,
                              g_variant_get_fixed_array(v, &n_elem, 1), NULL);
        }
        ++blob_id;
    }
}

} // namespace

PostProcFunction EXTRACT_INFERENCE_RESULTS = ExtractInferenceResults;
