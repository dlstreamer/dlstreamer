/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvaclassify.h"
#include "config.h"
#include "inference_impl.h"
#include "meta_converters.h"
#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gva_roi_meta.h>
#include <opencv2/imgproc.hpp>

using namespace InferenceBackend;

#define ELEMENT_LONG_NAME "Object classification (requires GstVideoRegionOfInterestMeta on input)"
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Post-processing

static void copy_buffer_to_structure(GstStructure *structure, const void *buffer, int size) {
    GVariant *v = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, buffer, size, 1);
    gsize n_elem;
    gst_structure_set(structure, "data_buffer", G_TYPE_VARIANT, v, "data", G_TYPE_POINTER,
                      g_variant_get_fixed_array(v, &n_elem, 1), NULL);
}

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

void Blob2RoiMeta(const std::map<std::string, OutputBlob::Ptr> &output_blobs, std::vector<InferenceROI> frames,
                  const std::map<std::string, GstStructure *> &model_proc, const gchar *model_name,
                  GvaBaseInference * /*gva_base_inference*/) {
    int batch_size = frames.size();

    for (auto blob_iter : output_blobs) {
        std::string layer_name = blob_iter.first;
        OutputBlob::Ptr blob = blob_iter.second;
        if (blob == nullptr)
            throw std::runtime_error("Blob is empty. Cannot access to null object.");

        const uint8_t *data = (const uint8_t *)blob->GetData();
        int size = get_unbatched_size_in_bytes(blob, batch_size);
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
                              model_name, "precision", G_TYPE_INT, (int)blob->GetPrecision(), "layout", G_TYPE_INT,
                              (int)blob->GetLayout(), "rank", G_TYPE_INT, rank, NULL);
            copy_buffer_to_structure(s, data + b * size, size);
            if (proc != model_proc.end()) {
                ConvertMeta(s);
            }
            gst_video_region_of_interest_meta_add_param(meta, s);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pre-processing

cv::Mat GetTransform(cv::Mat *src, cv::Mat *dst) {
    cv::Mat col_mean_src;
    reduce(*src, col_mean_src, 0, cv::REDUCE_AVG);
    for (int i = 0; i < src->rows; i++) {
        src->row(i) -= col_mean_src;
    }

    cv::Mat col_mean_dst;
    reduce(*dst, col_mean_dst, 0, cv::REDUCE_AVG);
    for (int i = 0; i < dst->rows; i++) {
        dst->row(i) -= col_mean_dst;
    }

    cv::Scalar mean, dev_src, dev_dst;
    cv::meanStdDev(*src, mean, dev_src);
    dev_src(0) = std::max(static_cast<double>(std::numeric_limits<float>::epsilon()), dev_src(0));
    *src /= dev_src(0);
    cv::meanStdDev(*dst, mean, dev_dst);
    dev_dst(0) = std::max(static_cast<double>(std::numeric_limits<float>::epsilon()), dev_dst(0));
    *dst /= dev_dst(0);

    cv::Mat w, u, vt;
    cv::SVD::compute((*src).t() * (*dst), w, u, vt);
    cv::Mat r = (u * vt).t();
    cv::Mat m(2, 3, CV_32F);
    m.colRange(0, 2) = r * (dev_dst(0) / dev_src(0));
    m.col(2) = (col_mean_dst.t() - m.colRange(0, 2) * col_mean_src.t());
    return m;
}

void align_rgb_image(Image &image, const std::vector<float> &landmarks_points,
                     const std::vector<float> &reference_points) {
    cv::Mat ref_landmarks = cv::Mat(reference_points.size() / 2, 2, CV_32F);
    cv::Mat landmarks =
        cv::Mat(landmarks_points.size() / 2, 2, CV_32F, const_cast<float *>(&landmarks_points.front())).clone();

    for (int i = 0; i < ref_landmarks.rows; i++) {
        ref_landmarks.at<float>(i, 0) = reference_points[2 * i] * image.width;
        ref_landmarks.at<float>(i, 1) = reference_points[2 * i + 1] * image.height;
        landmarks.at<float>(i, 0) *= image.width;
        landmarks.at<float>(i, 1) *= image.height;
    }
    cv::Mat m = GetTransform(&ref_landmarks, &landmarks);
    for (int plane_num = 0; plane_num < 4; plane_num++) {
        if (image.planes[plane_num]) {
            cv::Mat mat0(image.height, image.width, CV_8UC1, image.planes[plane_num], image.stride[plane_num]);
            cv::warpAffine(mat0, mat0, m, mat0.size(), cv::WARP_INVERSE_MAP);
        }
    }
}

std::function<void(Image &)> InputPreProcess(GstStructure *preproc, GstVideoRegionOfInterestMeta *roi_meta) {
    const gchar *converter = preproc ? gst_structure_get_string(preproc, "converter") : "";
    if (std::string(converter) == "alignment") {
        std::vector<float> reference_points;
        std::vector<float> landmarks_points;
        // look for tensor data with corresponding format
        GVA::RegionOfInterest roi(roi_meta);
        for (auto tensor : roi) {
            if (tensor.get_string("format") == "landmark_points") {
                landmarks_points = tensor.data<float>();
                break;
            }
        }
        // load reference points from JSON input_preproc description
        GValueArray *alignment_points = nullptr;
        if (gst_structure_get_array(preproc, "alignment_points", &alignment_points)) {
            for (size_t i = 0; i < alignment_points->n_values; i++) {
                reference_points.push_back(g_value_get_double(alignment_points->values + i));
            }
            G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            g_value_array_free(alignment_points);
            G_GNUC_END_IGNORE_DEPRECATIONS
        }

        if (landmarks_points.size() && landmarks_points.size() == reference_points.size()) {
            return [reference_points, landmarks_points](Image &picture) {
                align_rgb_image(picture, landmarks_points, reference_points);
            };
        }
    }
    return [](Image &) {};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Register element

extern "C" {

GST_DEBUG_CATEGORY_STATIC(gst_gva_classify_debug_category);
#define GST_CAT_DEFAULT gst_gva_classify_debug_category

G_DEFINE_TYPE_WITH_CODE(GstGvaClassify, gst_gva_classify, GST_TYPE_GVA_BASE_INFERENCE,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_classify_debug_category, "gvaclassify", 0,
                                                "debug category for gvaclassify element"));

void gst_gva_classify_class_init(GstGvaClassifyClass *klass) {
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(VIDEO_SRC_CAPS)));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(VIDEO_SINK_CAPS)));

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");
}

void gst_gva_classify_init(GstGvaClassify *gvaclassify) {
    GST_DEBUG_OBJECT(gvaclassify, "gst_gva_classify_init");
    GST_DEBUG_OBJECT(gvaclassify, "%s", GST_ELEMENT_NAME(GST_ELEMENT(gvaclassify)));

    gvaclassify->base_inference.is_full_frame = FALSE;

    GetROIPreProcFunction get_roi_pre_proc = InputPreProcess;
    gvaclassify->base_inference.get_roi_pre_proc = (void *)get_roi_pre_proc;

    PostProcFunction post_proc = Blob2RoiMeta;
    gvaclassify->base_inference.post_proc = (void *)post_proc;
}

} // extern "C"
