/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "blob2metadata.h"
#include "gva_roi_meta.h"
#include "gva_tensor_meta.h"
#include "meta_converters.h"

#include <inference_engine.hpp>

using namespace InferenceBackend;

int GetUnbatchedSizeInBytes(OutputBlob::Ptr blob, size_t batch_size) {
    const std::vector<size_t> &dims = blob->GetDims();
    if (dims[0] != batch_size) {
        throw std::logic_error("Blob last dimension should be equal to batch size");
    }
    int size = dims[1];
    for (size_t i = 2; i < dims.size(); i++) {
        size *= dims[i];
    }
    switch (blob->GetPrecision()) {
    case OutputBlob::Precision::FP32:
        size *= sizeof(float);
        break;
    case OutputBlob::Precision::U8:
        break;
    }
    return size;
}

void Blob2TensorMeta(const std::map<std::string, OutputBlob::Ptr> &output_blobs, std::vector<InferenceFrame> frames,
                     const gchar *inference_id, const gchar *model_name) {
    int batch_size = frames.size();

    for (auto blob_iter : output_blobs) {
        const char *layer_name = blob_iter.first.c_str();
        OutputBlob::Ptr blob = blob_iter.second;
        const uint8_t *data = (const uint8_t *)blob->GetData();
        auto dims = blob->GetDims();
        int size = GetUnbatchedSizeInBytes(blob, batch_size);

        for (int b = 0; b < batch_size; b++) {
            InferenceFrame &frame = frames[b];

            // find or create new meta
            GstGVATensorMeta *meta = find_tensor_meta_ext(frame.buffer, model_name, layer_name, inference_id);
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
                meta->element_id = inference_id;
                meta->total_bytes = size * meta->dims[0];
                meta->data = g_slice_alloc0(meta->total_bytes);
            }
            memcpy(meta->data, data + b * size, size);
        }
    }
}

void copy_buffer_to_structure(GstStructure *structure, const void *buffer, int size) {
    GVariant *v = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, buffer, size, 1);
    gsize n_elem;
    gst_structure_set(structure, "data_buffer", G_TYPE_VARIANT, v, "data", G_TYPE_POINTER,
                      g_variant_get_fixed_array(v, &n_elem, 1), NULL);
}

void Blob2RoiMeta(const std::map<std::string, OutputBlob::Ptr> &output_blobs, std::vector<InferenceFrame> frames,
                  const gchar *inference_id, const gchar *model_name,
                  const std::map<std::string, GstStructure *> &model_proc) {
    int batch_size = frames.size();

    for (auto blob_iter : output_blobs) {
        std::string layer_name = blob_iter.first;
        OutputBlob::Ptr blob = blob_iter.second;
        const uint8_t *data = (const uint8_t *)blob->GetData();
        int size = GetUnbatchedSizeInBytes(blob, batch_size);
        int rank = (int)blob->GetDims().size();

        for (int b = 0; b < batch_size; b++) {
            // find meta
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

            // add new structure to meta
            GstStructure *s;
            auto proc = model_proc.find(layer_name);
            if (proc != model_proc.end()) {
                s = gst_structure_copy(proc->second);
            } else {
                s = gst_structure_new_empty(("layer:" + layer_name).data());
            }
            gst_structure_set(s, "layer_name", G_TYPE_STRING, layer_name.data(), "model_name", G_TYPE_STRING,
                              model_name, "element_id", G_TYPE_STRING, inference_id, "precision", G_TYPE_INT,
                              (int)blob->GetPrecision(), "layout", G_TYPE_INT, (int)blob->GetLayout(), "rank",
                              G_TYPE_INT, rank, NULL);
            copy_buffer_to_structure(s, data + b * size, size);
            if (proc != model_proc.end()) {
                ConvertMeta(s);
            }
            gst_video_region_of_interest_meta_add_param(meta, s);
        }
    }
}
