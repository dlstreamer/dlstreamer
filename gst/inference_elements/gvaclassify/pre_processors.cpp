/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <map>
#include <string>
#include <vector>

#include "classification_history.h"
#include "gstgvaclassify.h"
#include "gva_base_inference.h"
#include "gva_utils.h"
#include "region_of_interest.h"

#include <inference_backend/image_inference.h>
#include <opencv2/imgproc.hpp>

#include "pre_processors.h"

namespace {

using namespace InferenceBackend;

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

// void rezise_image(const cv::Mat& image, uint8_t* buffer) const {

//     cv::Mat resizedImage;
//     double scale = inputLayerSize.height / static_cast<double>(image.rows);
//     cv::resize(image, resizedImage, cv::Size(), scale, scale, cv::INTER_CUBIC);
//     cv::Mat paddedImage;
//     cv::copyMakeBorder(resizedImage, paddedImage, pad(0), pad(2), pad(1), pad(3),
//                        cv::BORDER_CONSTANT, meanPixel);
//     std::vector<cv::Mat> planes(3);
//     for (size_t pId = 0; pId < planes.size(); pId++) {
//         planes[pId] = cv::Mat(inputLayerSize, CV_8UC1,
//                               buffer + pId * inputLayerSize.area());
//     }
//     cv::split(paddedImage, planes);
// }



bool IsROIClassificationNeeded(GvaBaseInference *gva_base_inference, guint current_num_frame, GstBuffer * /* *buffer*/,
                               GstVideoRegionOfInterestMeta *roi) {
    GstGvaClassify *gva_classify = (GstGvaClassify *)gva_base_inference;

    // Check is object-class same with roi type
    if (gva_classify->object_class[0]) {
        static std::map<std::string, std::vector<std::string>> elemets_object_classes;
        auto it = elemets_object_classes.find(gva_base_inference->inference_id);
        if (it == elemets_object_classes.end())
            it = elemets_object_classes.insert(
                it, {gva_base_inference->inference_id, SplitString(gva_classify->object_class, ',')});

        auto compare_quark_string = [roi](const std::string &str) {
            const gchar *roi_type = roi->roi_type ? g_quark_to_string(roi->roi_type) : "";
            return (strcmp(roi_type, str.c_str()) == 0);
        };
        if (std::find_if(it->second.cbegin(), it->second.cend(), compare_quark_string) == it->second.cend()) {
            return false;
        }
    }

    // Check is object recently classified
    return (!gva_classify->skip_classified_objects or
            gva_classify->classification_history->IsROIClassificationNeeded(roi, current_num_frame));
}

RoiPreProcessorFunction InputPreProcess(GstStructure *preproc, GstVideoRegionOfInterestMeta *roi_meta) {
    const gchar *converter = preproc ? gst_structure_get_string(preproc, "converter") : "";
    if (std::string(converter) == "alignment") {
        std::vector<float> reference_points;
        std::vector<float> landmarks_points;
        // look for tensor data with corresponding format
        GVA::RegionOfInterest roi(roi_meta);
        for (auto tensor : roi) {
            if (tensor.format() == "landmark_points") {
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

} // anonymous namespace

GetROIPreProcFunction INPUT_PRE_PROCESS = InputPreProcess;
IsROIClassificationNeededFunction IS_ROI_CLASSIFICATION_NEEDED = IsROIClassificationNeeded;
