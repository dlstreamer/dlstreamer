/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvainference.h"
#include "config.h"
#include "inference_impl.h"
#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gva_tensor_meta.h>
#include <opencv2/imgproc.hpp>

using namespace InferenceBackend;

#define ELEMENT_LONG_NAME "Generic full-frame inference (generates GstGVATensorMeta)"
#define ELEMENT_DESCRIPTION ELEMENT_LONG_NAME

#ifdef SUPPORT_DMA_BUFFER
#define DMA_BUFFER_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:DMABuf", "{ I420 }") "; "
#else
#define DMA_BUFFER_CAPS
#endif

#define VA_SURFACE_CAPS

#define SYSTEM_MEM_CAPS GST_VIDEO_CAPS_MAKE("{ BGRx, BGRA }")
#define INFERENCE_CAPS DMA_BUFFER_CAPS VA_SURFACE_CAPS SYSTEM_MEM_CAPS
#define VIDEO_SINK_CAPS INFERENCE_CAPS
#define VIDEO_SRC_CAPS INFERENCE_CAPS

static int get_unbatched_size_in_bytes(OutputBlob::Ptr blob, size_t batch_size) {
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

void Blob2TensorMeta(const std::map<std::string, OutputBlob::Ptr> &output_blobs, std::vector<InferenceROI> frames,
                     const std::map<std::string, GstStructure *> & /*model_proc*/, const gchar *model_name,
                     GvaBaseInference *gva_base_inference) {
    int batch_size = frames.size();

    for (auto blob_iter : output_blobs) {
        const char *layer_name = blob_iter.first.c_str();
        OutputBlob::Ptr blob = blob_iter.second;
        if (blob == nullptr)
            throw std::runtime_error("Blob is empty. Cannot access to null object.");

        const uint8_t *data = (const uint8_t *)blob->GetData();
        auto dims = blob->GetDims();
        int size = get_unbatched_size_in_bytes(blob, batch_size);

        for (int b = 0; b < batch_size; b++) {
            InferenceROI &frame = frames[b];

            // find or create new meta
            GstGVATensorMeta *meta =
                find_tensor_meta_ext(frame.buffer, model_name, layer_name, gva_base_inference->inference_id);
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
                meta->element_id = gva_base_inference->inference_id;
                meta->total_bytes = size * meta->dims[0];
                meta->data = g_slice_alloc0(meta->total_bytes);
            }
            memcpy(meta->data, data + b * size, size);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Register element

extern "C" {

GST_DEBUG_CATEGORY_STATIC(gst_gva_inference_debug_category);
#define GST_CAT_DEFAULT gst_gva_inference_debug_category

G_DEFINE_TYPE_WITH_CODE(GstGvaInference, gst_gva_inference, GST_TYPE_GVA_BASE_INFERENCE,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_inference_debug_category, "gvainference", 0,
                                                "debug category for gvainference element"));

void gst_gva_inference_class_init(GstGvaInferenceClass *klass) {
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(VIDEO_SRC_CAPS)));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(VIDEO_SINK_CAPS)));

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");
}

void gst_gva_inference_init(GstGvaInference *gvainference) {
    GST_DEBUG_OBJECT(gvainference, "gst_gva_inference_init");
    GST_DEBUG_OBJECT(gvainference, "%s", GST_ELEMENT_NAME(GST_ELEMENT(gvainference)));

    PostProcFunction post_proc = Blob2TensorMeta;
    gvainference->base_inference.post_proc = (void *)post_proc;
}

} // extern "C"
