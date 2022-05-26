/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __OT_YUV_IMAGE_H__
#define __OT_YUV_IMAGE_H__

#include <cstdint>
#include <opencv2/opencv.hpp>

// #define DBG_YUVIMAGE
#ifdef DBG_YUVIMAGE
#include <fstream>
#endif // DBG_YUVIMAGE

namespace vas {
namespace ot {

class YuvImage {
  public:
    enum Format { FMT_UNKNOWN = -1, FMT_NV12 = 0, FMT_GRAY, FMT_RGB24, FMT_BGR24, FMT_BGRA32, FMT_I420 };

    YuvImage();
    YuvImage(const YuvImage &src);

    YuvImage(const cv::Mat &bgraImage, Format fmt, int32_t index = 0);
    YuvImage(const cv::Mat &bgraImage, Format input_fmt, Format output_fmt,
             int32_t index = 0); // Do not use this function for 'uv_upsampled_ == true'

    YuvImage(int32_t width, int32_t height, bool uv_upsample = false, Format format = FMT_NV12,
             int32_t index = 0); // Valid for both 'uv_upsampled_ == true' or not
    YuvImage(int32_t width, int32_t height, int32_t stride_, uint8_t *data, Format format = FMT_NV12,
             int32_t index = 0); // Do not use this function if 'uv_upsampled_ == true'

    virtual ~YuvImage();

    YuvImage ToGray() const;

    /*
     * @param: cp           [input] center point in a source image
     *         crop_sz      [input] cropping size of a source image
     *         resize       [input] target size in the destination
     *         output_nv12  [input] Output color space. if true, then the output containter will be NV12 which uses
     * 'data_uv_' for UV domain. If false, then the output containter will be 'upsampled' like YUV444 which uses not
     * 'data_uv_' but 'data_u_' and 'data_v_'
     * @details
     *         Supported format: FMT_BGR24, FMT_RGB24, FMT_BGRA32
     *         If crop area is outside the source, adds padding to outside area and resize it.
     *         other format : throw exception(invalid_argument)
     */
    YuvImage CropAndResize(const cv::Point2f &cp, const cv::Size2f &crop_sz, const cv::Size &resize,
                           bool output_nv12 = false) const;

    // Supported format: FMT_NV12 / FMT_I420
    int32_t CropAndResizeNv12(const cv::Point2f &cp, const cv::Size2f &crop_size, YuvImage *dst) const;
    int32_t CropAndResizeI420(const cv::Point2f &cp, const cv::Size2f &crop_size, YuvImage *dst) const;

    // Supported format: FMT_BGR24, FMT_RGB24, FMT_BGRA32
    static int32_t Resize(const YuvImage &src, YuvImage *dst, cv::Size target_sz);

    // FMT_RGB24, FMT_BGR24(0 == red, 1 == green, 2 == blue) , FMT_NV12(0 == n, 1 == uv or 1 == u, 2 == v)
    uint8_t *GetChannelPtr(int32_t idx) const;

    YuvImage &operator=(const YuvImage &rhs) = delete;

  public:
    bool is_reference_;
    int32_t width_;
    int32_t height_;
    int32_t stride_;
    Format format_;
    bool uv_upsampled_;
    int32_t index_;
    size_t size_;

    uint8_t *data_;
    uint8_t *data_u_;
    uint8_t *data_v_;
    uint8_t *data_uv_;

    void Release();
    cv::Mat ToCVMat() const;
};

}; // namespace ot
}; // namespace vas

#endif // __OT_YUV_IMAGE_H__
