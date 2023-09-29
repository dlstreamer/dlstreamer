/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vas/components/ot/mtt/spatial_rgb_histogram.h"

#include "vas/components/ot/prof_def.h"

namespace vas {
namespace ot {

SpatialRgbHistogram::SpatialRgbHistogram(int32_t canonical_patch_size, int32_t spatial_bin_size,
                                         int32_t spatial_bin_stride, int32_t rgb_bin_size)
    : RgbHistogram(rgb_bin_size), canonical_patch_size_(canonical_patch_size), spatial_bin_size_(spatial_bin_size),
      spatial_bin_stride_(spatial_bin_stride),
      spatial_num_bins_(1 + (canonical_patch_size - spatial_bin_size) / spatial_bin_stride),
      spatial_hist_size_(spatial_num_bins_ * spatial_num_bins_ * rgb_hist_size_) {
    weight_.create(cv::Size(canonical_patch_size, canonical_patch_size), CV_32F);
    const float sigma = 0.5f * canonical_patch_size;
    for (int32_t y = 0; y < canonical_patch_size; ++y) {
        for (int32_t x = 0; x < canonical_patch_size; ++x) {
            float dx = (0.5f * canonical_patch_size - x) / sigma;
            float dy = (0.5f * canonical_patch_size - y) / sigma;
            weight_.at<float>(y, x) = expf(-0.5f * (dx * dx + dy * dy));
        }
    }
}

SpatialRgbHistogram::~SpatialRgbHistogram(void) {
    weight_.release();
}

void SpatialRgbHistogram::Compute(const cv::Mat &image, cv::Mat *hist) {
    PROF_START(PROF_COMPONENTS_OT_SHORTTERM_COMPUTE_HIST);
    // Init output buffer
    hist->create(1, spatial_hist_size_, CV_32F);
    (*hist) = cv::Scalar(0);
    float *rgb_hist_ptr = hist->ptr<float>();

    // Normalize input image into canonical size
    if (image.rows <= 0 || image.cols <= 0) {
        PROF_END(PROF_COMPONENTS_OT_SHORTTERM_COMPUTE_HIST);
        return;
    }
    cv::Mat patch;
    cv::resize(image, patch, cv::Size(canonical_patch_size_, canonical_patch_size_));

#if 0 // Debugging
    const char* window_name = "Debugging ComputeFromNv12";
    cv::namedWindow(window_name, cv::WINDOW_NORMAL | cv::WINDOW_KEEPRATIO);
    cv::imshow(window_name, patch);
    cv::waitKey(0);
#endif

    // Compute spatial histogram
    for (int32_t y_bin = 0; y_bin < spatial_num_bins_; ++y_bin) {
        int32_t y = y_bin * spatial_bin_stride_;
        for (int32_t x_bin = 0; x_bin < spatial_num_bins_; ++x_bin) {
            // Compute RGB histogram per each spatial bin
            int32_t x = x_bin * spatial_bin_stride_;

            // AccumulateRgbHistogram(patch(cv::Rect(x, y, spatial_bin_size_, spatial_bin_size_)), rgb_hist_ptr);
            AccumulateRgbHistogram(patch(cv::Rect(x, y, spatial_bin_size_, spatial_bin_size_)),
                                   weight_(cv::Rect(x, y, spatial_bin_size_, spatial_bin_size_)), rgb_hist_ptr);
            rgb_hist_ptr += rgb_hist_size_;
        }
    }

    PROF_END(PROF_COMPONENTS_OT_SHORTTERM_COMPUTE_HIST);
}

void SpatialRgbHistogram::ComputeFromBgra32(const cv::Mat &image, cv::Mat *hist) {
    PROF_START(PROF_COMPONENTS_OT_SHORTTERM_COMPUTE_HIST);
    // Init output buffer
    hist->create(1, spatial_hist_size_, CV_32F);
    (*hist) = cv::Scalar(0);
    float *rgb_hist_ptr = hist->ptr<float>();

    // Normalize input image into canonical size
    if (image.rows <= 0 || image.cols <= 0) {
        PROF_END(PROF_COMPONENTS_OT_SHORTTERM_COMPUTE_HIST);
        return;
    }

    cv::Mat patch;
    cv::resize(image, patch, cv::Size(canonical_patch_size_, canonical_patch_size_));

#if 0 // Debugging
    cv::Mat debug_img;
    const char* window_name = "Debugging ComputeFromBgra32";
    cv::namedWindow(window_name, cv::WINDOW_NORMAL | cv::WINDOW_KEEPRATIO);
    cv::cvtColor(patch, debug_img, cv::COLOR_BGRA2BGR);
    cv::imshow(window_name, debug_img);
    cv::waitKey(0);
#endif

    // Compute spatial histogram
    for (int32_t y_bin = 0; y_bin < spatial_num_bins_; ++y_bin) {
        int32_t y = y_bin * spatial_bin_stride_;
        for (int32_t x_bin = 0; x_bin < spatial_num_bins_; ++x_bin) {
            // Compute RGB histogram per each spatial bin
            int32_t x = x_bin * spatial_bin_stride_;

            // AccumulateRgbHistogramFromBgra32(patch(cv::Rect(x, y, spatial_bin_size_, spatial_bin_size_)),
            // rgb_hist_ptr);
            AccumulateRgbHistogramFromBgra32(patch(cv::Rect(x, y, spatial_bin_size_, spatial_bin_size_)),
                                             weight_(cv::Rect(x, y, spatial_bin_size_, spatial_bin_size_)),
                                             rgb_hist_ptr);
            rgb_hist_ptr += rgb_hist_size_;
        }
    }

    PROF_END(PROF_COMPONENTS_OT_SHORTTERM_COMPUTE_HIST);
}

void SpatialRgbHistogram::ComputeFromNv12(const YuvImage &image, const cv::Rect &roi, cv::Mat *hist) {
    PROF_START(PROF_COMPONENTS_OT_SHORTTERM_COMPUTE_HIST);
    cv::Point2f cp(roi.x + roi.width / 2, roi.y + roi.height / 2);

    // Init output buffer
    hist->create(1, spatial_hist_size_, CV_32F);
    (*hist) = cv::Scalar(0);
    float *rgb_hist_ptr = hist->ptr<float>();

    // Normalize input image into canonical size
    if (image.height_ <= 0 || image.width_ <= 0) {
        PROF_END(PROF_COMPONENTS_OT_SHORTTERM_COMPUTE_HIST);
        return;
    }

    cv::Mat patch;
    YuvImage roi_patch(canonical_patch_size_, canonical_patch_size_, false);
    image.CropAndResizeNv12(cp, cv::Size2f(roi.width, roi.height), &roi_patch);
    cv::cvtColor(roi_patch.ToCVMat(), patch, cv::COLOR_YUV2BGR_NV12);

#if 0 // Debugging
    if (false)
    {
        char filename[256] = {};
        std::snprintf(filename, sizeof(filename), "C:/Users/byungilm/Desktop/vbshared/intel_exported_working/engine/imv-lib/out_win64_icc/nv12_%dx%d_%d.yuv", canonical_patch_size_, canonical_patch_size_, canonical_patch_size_);
        std::ofstream write_file(filename, std::ofstream::out | std::ofstream::binary);
        write_file.write(const_cast<char*>(reinterpret_cast<char*>(roi_patch.data_)), roi_patch.size_);
        write_file.close();
    }

    const char* window_name = "Debugging ComputeFromNv12";
    cv::namedWindow(window_name, cv::WINDOW_NORMAL | cv::WINDOW_KEEPRATIO);
    cv::imshow(window_name, patch);
    cv::waitKey(0);
#endif

    // Compute spatial histogram
    for (int32_t y_bin = 0; y_bin < spatial_num_bins_; ++y_bin) {
        int32_t y = y_bin * spatial_bin_stride_;
        for (int32_t x_bin = 0; x_bin < spatial_num_bins_; ++x_bin) {
            int32_t x = x_bin * spatial_bin_stride_;
            // Compute RGB histogram per each spatial bin
            AccumulateRgbHistogram(patch(cv::Rect(x, y, spatial_bin_size_, spatial_bin_size_)),
                                   weight_(cv::Rect(x, y, spatial_bin_size_, spatial_bin_size_)), rgb_hist_ptr);
            rgb_hist_ptr += rgb_hist_size_;
        }
    }

    PROF_END(PROF_COMPONENTS_OT_SHORTTERM_COMPUTE_HIST);
}

void SpatialRgbHistogram::ComputeFromI420(const YuvImage &image, const cv::Rect &roi, cv::Mat *hist) {
    PROF_START(PROF_COMPONENTS_OT_SHORTTERM_COMPUTE_HIST);
    cv::Point2f cp(roi.x + roi.width / 2, roi.y + roi.height / 2);

    // Init output buffer
    hist->create(1, spatial_hist_size_, CV_32F);
    (*hist) = cv::Scalar(0);
    float *rgb_hist_ptr = hist->ptr<float>();

    // Normalize input image into canonical size
    if (image.height_ <= 0 || image.width_ <= 0) {
        PROF_END(PROF_COMPONENTS_OT_SHORTTERM_COMPUTE_HIST);
        return;
    }

    cv::Mat patch;
    YuvImage roi_patch(canonical_patch_size_, canonical_patch_size_, false, YuvImage::FMT_I420);
    image.CropAndResizeI420(cp, cv::Size2f(roi.width, roi.height), &roi_patch);
    cv::cvtColor(roi_patch.ToCVMat(), patch, cv::COLOR_YUV2BGR_I420);

#if 0 // Debugging
    if (false)
    {
        char filename[256] = {};
        std::snprintf(filename, sizeof(filename), "C:/Users/byungilm/Desktop/vbshared/intel_exported_working/engine/imv-lib/out_win64_icc/nv12_%dx%d_%d.yuv", canonical_patch_size_, canonical_patch_size_, canonical_patch_size_);
        std::ofstream write_file(filename, std::ofstream::out | std::ofstream::binary);
        write_file.write(const_cast<char*>(reinterpret_cast<char*>(roi_patch.data_)), roi_patch.size_);
        write_file.close();
    }

    const char* window_name = "Debugging ComputeFromNv12";
    cv::namedWindow(window_name, cv::WINDOW_NORMAL | cv::WINDOW_KEEPRATIO);
    cv::imshow(window_name, patch);
    cv::waitKey(0);
#endif

    // Compute spatial histogram
    for (int32_t y_bin = 0; y_bin < spatial_num_bins_; ++y_bin) {
        int32_t y = y_bin * spatial_bin_stride_;
        for (int32_t x_bin = 0; x_bin < spatial_num_bins_; ++x_bin) {
            int32_t x = x_bin * spatial_bin_stride_;
            // Compute RGB histogram per each spatial bin
            AccumulateRgbHistogram(patch(cv::Rect(x, y, spatial_bin_size_, spatial_bin_size_)),
                                   weight_(cv::Rect(x, y, spatial_bin_size_, spatial_bin_size_)), rgb_hist_ptr);
            rgb_hist_ptr += rgb_hist_size_;
        }
    }

    PROF_END(PROF_COMPONENTS_OT_SHORTTERM_COMPUTE_HIST);
}

int32_t SpatialRgbHistogram::FeatureSize(void) const {
    return spatial_hist_size_;
}

}; // namespace ot
}; // namespace vas
