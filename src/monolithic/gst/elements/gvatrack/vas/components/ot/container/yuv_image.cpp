/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vas/components/ot/container/yuv_image.h"

#include "vas/common/exception.h"
#include <algorithm>
#include <cstring>
#include <opencv2/opencv.hpp>
#include <vector>

namespace vas {
namespace ot {

#if 1
#define LINEAR_CALC(val, aa, bb, cc, dd, x_diff, y_diff)                                                               \
    val = static_cast<uint8_t>(static_cast<int32_t>((aa) * (1024 - x_diff) * (1024 - y_diff) +                         \
                                                    (bb) * x_diff * (1024 - y_diff) +                                  \
                                                    (cc) * y_diff * (1024 - x_diff) + (dd) * x_diff * y_diff) >>       \
                               20);

#define REDUCE_CAL_NV12

// copied from PVL
int32_t YuvImage::CropAndResizeNv12(const cv::Point2f &cp, const cv::Size2f &crop_size, YuvImage *dst) const {
    int32_t x_diff = 0;
    int32_t y_diff = 0;
    int32_t x_src = 0;
    int32_t y_src = 0;
    int32_t tmpx = 0;
    int32_t tmpy = 0;

    int32_t crop_left = static_cast<int32_t>(std::roundf(cp.x - crop_size.width / 2.f));
    int32_t crop_top = static_cast<int32_t>(std::roundf(cp.y - crop_size.height / 2.f));
    int32_t crop_right = crop_left + static_cast<int32_t>(crop_size.width) - 1;
    int32_t crop_bottom = crop_top + static_cast<int32_t>(crop_size.height) - 1;

    int32_t xratio = ((crop_right - crop_left) << 10) / dst->width_;
    int32_t yratio = ((crop_bottom - crop_top) << 10) / dst->height_;

    int32_t y_plane[2][2] = {0};
    int32_t u_plane[2][2] = {0};
    int32_t v_plane[2][2] = {0};

    int32_t y, x;
    int32_t offsetY;
    int32_t offsetUV;
    int32_t y_final;
    int32_t u_final;
    int32_t v_final;

    uint8_t *dst_y = dst->data_;
    uint8_t *dst_uv = dst->data_uv_;
    uint8_t *src_y = data_;
    uint8_t *src_uv = data_uv_;

    std::vector<int32_t> uv_buffer;
    try {
        uv_buffer.resize(stride_, 0);
    } catch (std::bad_alloc const &) {
        return -2;
    }

    for (y = 0; y < (dst->height_ & ~1); ++y, tmpy += yratio) {
        y_src = tmpy >> 10;
        y_diff = tmpy - (y_src << 10);
        /*
         *   Y00  Y01  Y02  Y03
         *   Y10  Y11  Y12  Y13
         *   Y20  Y21  Y22  Y23
         *   ..
         *   Uv00 uV01 Uv02 Uv03
         *   Uv10 uV11 Uv12 Uv13
         */
        y_src = std::max<int32_t>(0, std::min<int32_t>(height_ - 1, crop_top + y_src));

        offsetY = y_src * stride_;
        offsetUV = ((y_src & (~1)) / 2) * stride_;

        tmpx = 0;
        for (x = 0; x < (dst->width_ & ~1); ++x, tmpx += xratio) {
            // tmpx = x * xratio;
            x_src = tmpx >> 10;
            x_diff = tmpx - (x_src << 10);
            x_src = std::max<int32_t>(0, std::min<int32_t>(width_ - 1, crop_left + x_src));

            // Y plane
            y_plane[0][0] = src_y[offsetY + x_src];
            y_plane[0][1] = src_y[offsetY + x_src + 1];
            y_plane[1][0] = src_y[offsetY + stride_ + x_src];
            y_plane[1][1] = src_y[offsetY + stride_ + x_src + 1];

            LINEAR_CALC(y_final, y_plane[0][0], y_plane[0][1], y_plane[1][0], y_plane[1][1], x_diff, y_diff);
            if (y_final > 255)
                y_final = 255;
            if (y_final < 0)
                y_final = 0;

            dst_y[y * dst->stride_ + x] = static_cast<uint8_t>(y_final); // set Y in dest array

#if !defined(REDUCE_CAL_NV12)
            if ((y & 1) || (x & 1))
                continue;

#endif
            // UV plane
            if ((y_src & 1) == 0) { // Even for Height
                if ((x_src & 1) == 0) {
                    // Even for Width
                    u_plane[0][0] = static_cast<int32_t>(src_uv[offsetUV + x_src]);
                    u_plane[0][1] = static_cast<int32_t>(src_uv[offsetUV + x_src]);
                    u_plane[1][0] = static_cast<int32_t>(src_uv[offsetUV + x_src]);
                    u_plane[1][1] = static_cast<int32_t>(src_uv[offsetUV + x_src]);

                    v_plane[0][0] = static_cast<int32_t>(src_uv[offsetUV + x_src + 1]);
                    v_plane[0][1] = static_cast<int32_t>(src_uv[offsetUV + x_src + 1]);
                    v_plane[1][0] = static_cast<int32_t>(src_uv[offsetUV + x_src + 1]);
                    v_plane[1][1] = static_cast<int32_t>(src_uv[offsetUV + x_src + 1]);
                } else {
                    // Odd for Width
                    u_plane[0][0] = static_cast<int32_t>(src_uv[offsetUV + x_src - 1]);
                    u_plane[0][1] = static_cast<int32_t>(src_uv[offsetUV + x_src + 1]);
                    u_plane[1][0] = static_cast<int32_t>(src_uv[offsetUV + x_src - 1]);
                    u_plane[1][1] = static_cast<int32_t>(src_uv[offsetUV + x_src + 1]);

                    v_plane[0][0] = static_cast<int32_t>(src_uv[offsetUV + x_src]);
                    v_plane[0][1] = static_cast<int32_t>(src_uv[offsetUV + x_src + 2]);
                    v_plane[1][0] = static_cast<int32_t>(src_uv[offsetUV + x_src]);
                    v_plane[1][1] = static_cast<int32_t>(src_uv[offsetUV + x_src + 2]);
                }
            } else { // Odd at Height
                if ((x_src & 1) == 0) {
                    // Even for Width
                    u_plane[0][0] = static_cast<int32_t>(src_uv[offsetUV + x_src]);
                    u_plane[0][1] = static_cast<int32_t>(src_uv[offsetUV + x_src]);
                    u_plane[1][0] = static_cast<int32_t>(src_uv[offsetUV + stride_ + x_src]);
                    u_plane[1][1] = static_cast<int32_t>(src_uv[offsetUV + stride_ + x_src]);

                    v_plane[0][0] = static_cast<int32_t>(src_uv[offsetUV + x_src + 1]);
                    v_plane[0][1] = static_cast<int32_t>(src_uv[offsetUV + x_src + 1]);
                    v_plane[1][0] = static_cast<int32_t>(src_uv[offsetUV + stride_ + x_src + 1]);
                    v_plane[1][1] = static_cast<int32_t>(src_uv[offsetUV + stride_ + x_src + 1]);
                } else {
                    // Odd for Width
                    u_plane[0][0] = static_cast<int32_t>(src_uv[offsetUV + x_src - 1]);
                    u_plane[0][1] = static_cast<int32_t>(src_uv[offsetUV + x_src + 1]);
                    u_plane[1][0] = static_cast<int32_t>(src_uv[offsetUV + stride_ + x_src - 1]);
                    u_plane[1][1] = static_cast<int32_t>(src_uv[offsetUV + stride_ + x_src + 1]);

                    v_plane[0][0] = static_cast<int32_t>(src_uv[offsetUV + x_src]);
                    v_plane[0][1] = static_cast<int32_t>(src_uv[offsetUV + x_src + 2]);
                    v_plane[1][0] = static_cast<int32_t>(src_uv[offsetUV + stride_ + x_src]);
                    v_plane[1][1] = static_cast<int32_t>(src_uv[offsetUV + stride_ + x_src + 2]);
                }
            }

            LINEAR_CALC(u_final, u_plane[0][0], u_plane[0][1], u_plane[1][0], u_plane[1][1], x_diff, y_diff);
            LINEAR_CALC(v_final, v_plane[0][0], v_plane[0][1], v_plane[1][0], v_plane[1][1], x_diff, y_diff);

#if !defined(REDUCE_CAL_NV12)
            if (u_final > 255)
                u_final = 255;
            if (u_final < 0)
                u_final = 0;

            dst_uv[(y / 2) * dst->stride_ + x] = static_cast<uint8_t>(u_final);

            if (v_final > 255)
                v_final = 255;
            if (v_final < 0)
                v_final = 0;

            dst_uv[(y / 2) * dst->stride_ + x + 1] = static_cast<uint8_t>(v_final);
#else
            if ((y & 1) == 1) {
                // Store and clear (second line)
                if ((x & 1) == 1) {
                    uv_buffer[x - 1] += static_cast<int32_t>(u_final);
                    uv_buffer[x] += static_cast<int32_t>(v_final);

                    dst_uv[(y / 2) * dst->stride_ + x - 1] = static_cast<uint8_t>(uv_buffer[x - 1] / 4);
                    dst_uv[(y / 2) * dst->stride_ + x] = static_cast<uint8_t>(uv_buffer[x] / 4);
                    uv_buffer[x - 1] = 0;
                    uv_buffer[x] = 0;
                } else {
                    uv_buffer[x] += static_cast<int32_t>(u_final);
                    uv_buffer[x + 1] += static_cast<int32_t>(v_final);
                }
            } else {
                // Sum(first line)
                if ((x & 1) == 1) {
                    uv_buffer[x - 1] += static_cast<int32_t>(u_final);
                    uv_buffer[x] += static_cast<int32_t>(v_final);
                } else {
                    uv_buffer[x] = static_cast<int32_t>(u_final);
                    uv_buffer[x + 1] = static_cast<int32_t>(v_final);
                }
            }
#endif
        }
    }

    return 0;
}

int32_t YuvImage::CropAndResizeI420(const cv::Point2f &cp, const cv::Size2f &crop_size, YuvImage *dst) const {
    int32_t crop_left = static_cast<int32_t>(std::roundf(cp.x - crop_size.width / 2.f));
    int32_t crop_top = static_cast<int32_t>(std::roundf(cp.y - crop_size.height / 2.f));
    int32_t crop_right = crop_left + static_cast<int32_t>(crop_size.width) - 1;
    int32_t crop_bottom = crop_top + static_cast<int32_t>(crop_size.height) - 1;
    cv::Rect crop_rect(crop_left, crop_top, crop_right - crop_left + 1, crop_bottom - crop_top + 1);
    cv::Mat crop(cv::Size(crop_rect.width, crop_rect.height), CV_8UC1);
    cv::Rect crop_rect_uv(crop_left / 2, crop_top / 2, (crop_right - crop_left + 1) / 2,
                          (crop_bottom - crop_top + 1) / 2);
    cv::Mat crop_uv(cv::Size(crop_rect.width / 2, crop_rect.height / 2), CV_8UC1);

    int32_t border_left = 0;
    int32_t border_right = 0;
    int32_t border_top = 0;
    int32_t border_bottom = 0;
    if (crop_left < 0 || crop_top < 0 || crop_right >= width_ || crop_bottom >= height_) {
        if (crop_left < 0) {
            crop_rect.width = crop_rect.width + crop_left;
            border_left = -1 * crop_left;
            crop_rect.x = 0;
        }
        if (crop_top < 0) {
            crop_rect.height = crop_rect.height + crop_top;
            border_top = -1 * crop_top;
            crop_rect.y = 0;
        }
        if (crop_right >= width_) {
            crop_rect.width = crop_rect.width - (crop_right - width_ + 1);
            border_right = crop_right - width_ + 1;
        }
        if (crop_bottom >= height_) {
            crop_rect.height = crop_rect.height - (crop_bottom - height_ + 1);
            border_bottom = crop_bottom - height_ + 1;
        }
    }

    int32_t y_size = this->width_ * this->height_;
    int32_t uv_size = this->width_ * this->height_ / 4;
    auto data_u = dst->data_ + (dst->height_ * dst->width_);
    auto data_v = data_u + (dst->height_ * dst->width_) / 4;

    // Reference input/output
    cv::Mat src = this->ToCVMat();
    cv::Mat u_plane(this->height_ / 2, this->width_ / 2, CV_8UC1, this->data_ + y_size);
    cv::Mat v_plane(this->height_ / 2, this->width_ / 2, CV_8UC1, this->data_ + y_size + uv_size);
    cv::Mat result(dst->height_, dst->width_, CV_8UC1, dst->data_);
    cv::Mat result_u(dst->height_ / 2, dst->width_ / 2, CV_8UC1, data_u);
    cv::Mat result_v(dst->height_ / 2, dst->width_ / 2, CV_8UC1, data_v);

    // Y
    copyMakeBorder(src(crop_rect), crop, border_top, border_bottom, border_left, border_right, cv::BORDER_CONSTANT,
                   cv::Scalar(0, 0, 0));
    cv::resize(crop, result, cv::Size(dst->width_, dst->height_), 0, 0, cv::INTER_LINEAR);

    // UV
    cv::resize(u_plane(crop_rect_uv), result_u, cv::Size(dst->width_ / 2, dst->height_ / 2), 0, 0, cv::INTER_LINEAR);
    cv::resize(v_plane(crop_rect_uv), result_v, cv::Size(dst->width_ / 2, dst->height_ / 2), 0, 0, cv::INTER_LINEAR);

    return 0;
}
#endif // 0

int32_t YuvImage::Resize(const YuvImage &src, YuvImage *dst, cv::Size target_sz) {
    int32_t matching_type = CV_8UC3; // Default: FMT_BGR24
    int32_t num_chan = 3;            // Default: FMT_BGR24
    if (src.format_ == YuvImage::FMT_BGRA32) {
        matching_type = CV_8UC4;
        num_chan = 4;
    } else if (src.format_ == YuvImage::FMT_GRAY) {
        matching_type = CV_8UC1;
        num_chan = 1;
    }

    if (src.format_ == YuvImage::FMT_BGR24 || src.format_ == YuvImage::FMT_RGB24 ||
        src.format_ == YuvImage::FMT_BGRA32 || src.format_ == YuvImage::FMT_GRAY) {
        ETHROW(dst->data_ == nullptr, invalid_argument, "destination image is not empty");

        dst->width_ = target_sz.width;
        dst->height_ = target_sz.height;
        dst->stride_ = dst->width_ * num_chan;
        dst->size_ = dst->stride_ * dst->height_;
        // this won't be converted to smartpointer because of compatibility with OpenCV
        dst->data_ = new uint8_t[dst->size_];
        dst->format_ = src.format_;
        dst->index_ = src.index_;
        dst->uv_upsampled_ = false;
        dst->is_reference_ = false;
        dst->data_uv_ = nullptr;
        dst->data_u_ = nullptr;
        dst->data_v_ = nullptr;

        cv::Mat mat_src(cv::Size(src.width_, src.height_), matching_type, src.data_);
        cv::Mat mat_dst(target_sz, matching_type, dst->data_);
        cv::resize(mat_src, mat_dst, target_sz, 0, 0, cv::INTER_LINEAR_EXACT);
        return 0;
    }

    TRACE("not supported format");
    return -1;
}

YuvImage::YuvImage()
    : is_reference_(false), width_(-1), height_(-1), stride_(-1), format_(FMT_UNKNOWN), uv_upsampled_(false), index_(0),
      size_(0), data_(nullptr), data_u_(nullptr), data_v_(nullptr), data_uv_(nullptr) {
}

YuvImage::YuvImage(const cv::Mat &input_image, Format fmt, int32_t index)
    : is_reference_(false), format_(fmt), uv_upsampled_(false), index_(index) {
    ETHROW(input_image.data != nullptr, invalid_argument, "Invalid input YuvImage");

    if (fmt == FMT_NV12) {
        cv::Mat yuv_img;
        width_ = input_image.cols;
        height_ = input_image.rows * 2 / 3;
        stride_ = (width_ + 1) & ~1;

        int32_t aligned_h = (height_ + 1) & ~1;
        uint32_t uv_size = (stride_ / 2) * (aligned_h / 2);
        uint32_t y_size = stride_ * height_;

        size_ = y_size + uv_size * 2;

        // YUV channel
        is_reference_ = true;
        data_ = input_image.data;
        data_uv_ = data_ + y_size;

        data_u_ = nullptr;
        data_v_ = nullptr;
    } else if (fmt == FMT_I420) {
        cv::Mat yuv_img;
        width_ = input_image.cols;
        height_ = input_image.rows * 2 / 3;
        stride_ = (width_ + 1) & ~1;

        int32_t aligned_h = (height_ + 1) & ~1;
        uint32_t uv_size = (stride_ / 2) * (aligned_h / 2);
        uint32_t y_size = stride_ * height_;

        size_ = y_size + uv_size * 2;

        // YUV channel
        is_reference_ = true;
        data_ = input_image.data;
        data_uv_ = nullptr;

        data_u_ = data_ + y_size;
        data_v_ = data_u_ + uv_size;
    } else if (fmt == FMT_RGB24) {
        width_ = input_image.cols;
        height_ = input_image.rows;
        stride_ = static_cast<int32_t>(input_image.step[0]);
        size_ = stride_ * height_;
        is_reference_ = true;
        data_ = input_image.data;
        data_uv_ = nullptr;
        data_u_ = nullptr;
        data_v_ = nullptr;
    } else if (fmt == FMT_BGR24) {
        width_ = input_image.cols;
        height_ = input_image.rows;
        stride_ = static_cast<int32_t>(input_image.step[0]);
        size_ = stride_ * height_;
        is_reference_ = true;
        data_ = input_image.data;
        data_uv_ = nullptr;
        data_u_ = nullptr;
        data_v_ = nullptr;
    } else if (fmt == FMT_BGRA32) {
        width_ = input_image.cols;
        height_ = input_image.rows;
        stride_ = static_cast<int32_t>(input_image.step[0]);
        size_ = stride_ * height_;
        is_reference_ = true;
        data_ = input_image.data;
        data_uv_ = nullptr;
        data_u_ = nullptr;
        data_v_ = nullptr;
    } else {
        // Gray?
        width_ = input_image.cols;
        height_ = input_image.rows;
        stride_ = static_cast<int32_t>(input_image.step[0]);
        size_ = stride_ * height_;
        is_reference_ = true;
        data_ = input_image.data;
        data_uv_ = nullptr;
        data_u_ = nullptr;
        data_v_ = nullptr;
    }
}

// new operators in this constructor won't be converted to smartpointer because of compatibility with OpenCV
YuvImage::YuvImage(int32_t width, int32_t height, bool uv_upsample, Format format, int32_t index)
    : is_reference_(false), width_(width), height_(height), format_(format), uv_upsampled_(uv_upsample), index_(index),
      data_u_(nullptr), data_v_(nullptr), data_uv_(nullptr) {
    if (format_ == FMT_NV12) {
        stride_ = (width_ + 1) & ~1; // make it even
        if (uv_upsampled_ == false) {
            uint32_t aligned_h = (height_ + 1) & ~1; // make it even
            size_ = stride_ * (aligned_h * 3 / 2);
            data_ = new uint8_t[size_];
            data_uv_ = data_ + height_ * stride_;
        } else {
            size_ = stride_ * height_ * 3;
            data_ = new uint8_t[size_];
            data_uv_ = nullptr; // data_ + height_ * stride_;
            data_u_ = data_ + stride_ * height_;
            data_v_ = data_u_ + stride_ * height_;
        }
    } else if (format_ == FMT_I420) {
        stride_ = (width_ + 1) & ~1;             // make it even
        uint32_t aligned_h = (height_ + 1) & ~1; // make it even
        size_ = stride_ * (aligned_h * 3 / 2);
        data_ = new uint8_t[size_];
        data_u_ = data_ + height_ * stride_;
        data_v_ = data_ + (height_ * stride_) / 4;
    } else if (format_ == FMT_GRAY) {
        stride_ = (width_ + 1) & ~1; // make it even
        size_ = stride_ * height_;
        data_ = new uint8_t[size_];
        data_uv_ = nullptr;
    } else if (format_ == FMT_RGB24 || format_ == FMT_BGR24) {
        stride_ = width * 3;
        size_ = stride_ * height_;
        data_ = new uint8_t[size_];
        data_uv_ = nullptr;
    } else if (format_ == FMT_BGRA32) {
        stride_ = width * 4;
        size_ = stride_ * height_;
        data_ = new uint8_t[size_];
        data_uv_ = nullptr;
    } else {
        stride_ = width;
        ;
        size_ = stride_ * height_;
        data_ = new uint8_t[size_];
        data_uv_ = nullptr;
    }

    std::memset(data_, 0, size_);
}

// Do not use this function if 'uv_upsampled_ == true'
YuvImage::YuvImage(int32_t width, int32_t height, int32_t stride, uint8_t *data, Format format, int32_t index)
    : is_reference_(true), width_(width), height_(height), stride_(stride), format_(format), uv_upsampled_(false),
      index_(index), data_(data), data_u_(nullptr), data_v_(nullptr), data_uv_(nullptr) {
    if (format_ == FMT_NV12) {
        if (width_ % 2)
            stride_ = width_ + 1;

        data_uv_ = data_ + height_ * stride_;
        size_ = stride_ * height_ * 3 / 2;
    } else if (format_ == FMT_I420) {
        if (width_ % 2)
            stride_ = width_ + 1;

        data_u_ = data_ + height_ * stride_;
        data_v_ = data_ + (height_ * stride_) / 4;
        size_ = stride_ * height_ * 3 / 2;
    } else if (format_ == FMT_RGB24 || format_ == FMT_BGR24) {
        size_ = stride_ * height_;
    } else if (format_ == FMT_BGRA32) {
        size_ = stride_ * height_;
    } else if (format_ == FMT_GRAY) {
        if (width_ % 2)
            stride_ = width_ + 1;

        size_ = stride_ * height_;
    } else {
        // TRACE("Unexpected");
        size_ = 0;
    }
}

YuvImage::~YuvImage() {
    if (!is_reference_ && data_ != nullptr) {
        delete[] data_;
        data_ = nullptr;
        data_uv_ = nullptr;
    }
}

YuvImage YuvImage::ToGray() const {
    YuvImage gray(width_, height_, false, FMT_GRAY);
    if (format_ == FMT_BGR24) {
        cv::Mat color_mat(cv::Size(width_, height_), CV_8UC3, data_);
        cv::Mat gray_mat(cv::Size(width_, height_), CV_8UC1, gray.data_);
        cv::cvtColor(color_mat, gray_mat, cv::COLOR_BGR2GRAY);
    } else if (format_ == FMT_RGB24) {
        cv::Mat color_mat(cv::Size(width_, height_), CV_8UC3, data_);
        cv::Mat gray_mat(cv::Size(width_, height_), CV_8UC1, gray.data_);
        cv::cvtColor(color_mat, gray_mat, cv::COLOR_RGB2GRAY);
    } else if (format_ == FMT_BGRA32) {
        cv::Mat color_mat(cv::Size(width_, height_), CV_8UC4, data_);
        cv::Mat gray_mat(cv::Size(width_, height_), CV_8UC1, gray.data_);
        cv::cvtColor(color_mat, gray_mat, cv::COLOR_RGBA2GRAY);
    } else {
        TRACE("ToGray from format %d is not supported yet", format_);
    }

    return gray;
}

#if 0  // replaced with CropAndResize()
YuvImage YuvImage::CropAndResizeWithUpsampleUV(const cv::Point2f& cp, const cv::Size2f& size, const cv::Size& resize) const
{
    int32_t crop_left = static_cast<int32_t>(std::roundf(cp.x - size.width / 2.f));
    int32_t crop_top = static_cast<int32_t>(std::roundf(cp.y - size.height / 2.f));
    int32_t crop_right = static_cast<int32_t>(std::roundf(crop_left + size.width));
    int32_t crop_bottom = static_cast<int32_t>(std::roundf(crop_top + size.height));

    int32_t crop_w = crop_right - crop_left;
    int32_t crop_h = crop_bottom - crop_top;

    YuvImage resized(resize.width, resize.height, format_ == FMT_NV12 ? true : false, format_, index_);

    if (resize.area() == 0)
        return resized;

    int32_t ratio_w = (crop_w << 10) / resize.width;
    int32_t ratio_h = (crop_h << 10) / resize.height;

    if (format_ == FMT_NV12)
    {
        for (int32_t y = 0; y < resized.height_; ++y)
        {
            // repeat edge pixels for out of bound area
            int32_t sy_in = crop_top + ((ratio_h * y) >> 10);
            sy_in = std::max<int32_t>(0, std::min<int32_t>(height_ - 1, sy_in));
            uint8_t* src_y = data_ + sy_in * stride_;
            uint8_t* src_uv = data_uv_ + (sy_in & ~1) / 2 * stride_;
            uint8_t* dst_y = resized.data_ + y * resized.stride_;
            uint8_t* dst_u = resized.data_u_ + y * resized.stride_;
            uint8_t* dst_v = resized.data_v_ + y * resized.stride_;
            for (int32_t x = 0; x < resized.width_; ++x)
            {
                // repeat edge pixels for out of bound area
                int32_t sx_in = crop_left + ((ratio_w * x) >> 10);
                sx_in = std::max<int32_t>(0, std::min<int32_t>(width_ - 1, sx_in));

                dst_y[x] = src_y[sx_in];
                dst_u[x] = src_uv[sx_in & ~1];
                dst_v[x] = src_uv[(sx_in & ~1) + 1];
            }
        }
    }
    else if (format_ == FMT_RGB24)
    {
        for (int32_t y = 0; y < resized.height_; ++y)
        {
            // repeat edge pixels for out of bound area
            int32_t sy_in = crop_top + ((ratio_h * y) >> 10);
            sy_in = std::max<int32_t>(0, std::min<int32_t>(height_ - 1, sy_in));
            uint8_t* src = data_ + sy_in * stride_;
            uint8_t* dst = resized.data_ + y * resized.stride_;
            for (int32_t x = 0; x < resized.width_; ++x)
            {
                // repeat edge pixels for out of bound area
                int32_t sx_in = crop_left + ((ratio_w * x) >> 10);
                sx_in = std::max<int32_t>(0, std::min<int32_t>(width_ - 1, sx_in) * 3);

                dst[x * 3] = src[sx_in];
                dst[x * 3 + 1] = src[sx_in + 1];
                dst[x * 3 + 2] = src[sx_in + 2];
            }
        }
    }

    return resized;
}
#endif // 0

YuvImage YuvImage::CropAndResize(const cv::Point2f &cp, const cv::Size2f &crop_sz, const cv::Size &resize,
                                 bool output_nv12) const {
    int32_t crop_left = static_cast<int32_t>(std::roundf(cp.x - crop_sz.width / 2.f));
    int32_t crop_top = static_cast<int32_t>(std::roundf(cp.y - crop_sz.height / 2.f));
    int32_t crop_right = static_cast<int32_t>(std::roundf(crop_left + crop_sz.width));
    int32_t crop_bottom = static_cast<int32_t>(std::roundf(crop_top + crop_sz.height));
    int32_t crop_w = crop_right - crop_left;
    int32_t crop_h = crop_bottom - crop_top;

    bool output_upsample = false; // For yuv
    if (format_ == FMT_NV12)
        output_upsample = (output_nv12 == true) ? false : true;

    YuvImage output(resize.width, resize.height, output_upsample, format_, index_);

    ETHROW(resize.area() != 0, invalid_argument, "Invalid target size in CropAndResize");

    if (crop_w <= 0 || crop_h <= 0 || (crop_left < 0 && crop_right < 0) ||
        (crop_left > width_ - 1 && crop_right > width_ - 1) || (crop_top < 0 && crop_bottom < 0) ||
        (crop_top > height_ - 1 && crop_bottom > height_ - 1))
        return output;

    int32_t matching_type = CV_8UC3;
    if (format_ == FMT_BGRA32)
        matching_type = CV_8UC4;

    if (format_ == FMT_NV12) // Up-sampled NV12
    {
        int32_t ratio_w = (crop_w << 10) / resize.width;
        int32_t ratio_h = (crop_h << 10) / resize.height;
        if (uv_upsampled_ == false) // NV12
        {
            if (output_upsample == true) { // YUV444
                for (int32_t y = 0; y < output.height_; y++) {
                    // repeat edge pixels for out of bound area
                    int32_t sy_in = crop_top + ((ratio_h * y) >> 10);
                    sy_in = std::max<int32_t>(0, std::min<int32_t>(height_ - 1, sy_in));

                    uint8_t *src_y = data_ + sy_in * stride_;
                    uint8_t *src_uv = data_uv_ + (sy_in & ~1) / 2 * stride_;
                    uint8_t *dst_y = output.data_ + y * output.stride_;
                    uint8_t *dst_u = output.data_u_ + y * output.stride_;
                    uint8_t *dst_v = output.data_v_ + y * output.stride_;
                    for (int32_t x = 0; x < output.width_; x++) {
                        // repeat edge pixels for out of bound area
                        int32_t sx_in = crop_left + ((ratio_w * x) >> 10);
                        sx_in = std::max<int32_t>(0, std::min<int32_t>(width_ - 1, sx_in));

                        dst_y[x] = src_y[sx_in];
                        dst_u[x] = src_uv[sx_in & ~1];
                        dst_v[x] = src_uv[(sx_in & ~1) + 1];
                    }
                }
            } else // NV12
            {
                for (int32_t y = 0; y < output.height_; y++) {
                    // repeat edge pixels for out of bound area
                    int32_t sy_in = crop_top + ((ratio_h * y) >> 10);
                    sy_in = std::max<int32_t>(0, std::min<int32_t>(height_ - 1, sy_in));

                    uint8_t *src_y = data_ + sy_in * stride_;
                    uint8_t *src_uv = data_uv_ + (sy_in & ~1) / 2 * stride_;
                    uint8_t *dst_y = output.data_ + y * output.stride_;
                    uint8_t *dst_uv = output.data_uv_ + (y & ~1) / 2 * output.stride_;
                    for (int32_t x = 0; x < output.width_; ++x) {
                        // repeat edge pixels for out of bound area
                        int32_t sx_in = crop_left + ((ratio_w * x) >> 10);
                        sx_in = std::max<int32_t>(0, std::min<int32_t>(width_ - 1, sx_in));

                        dst_y[x] = src_y[sx_in];
                        if ((y & 1) || (x & 1)) {
                            continue;
                        } else {
                            dst_uv[x] = src_uv[sx_in & ~1];
                            dst_uv[x + 1] = src_uv[(sx_in & ~1) + 1];
                        }
                    }
                }
            }
        } else // YUV444
        {
            // 'uv_sampled' source. The 'output' will use 'data_u_' and 'data_v_' like YUV444
            for (int32_t y = 0; y < output.height_; y++) {
                // repeat edge pixels for out of bound area
                int32_t sy_in = crop_top + ((ratio_h * y) >> 10);
                sy_in = std::max<int32_t>(0, std::min<int32_t>(height_ - 1, sy_in));
                uint8_t *src_y = data_ + sy_in * stride_;
                uint8_t *src_u = data_u_ + sy_in * stride_;
                uint8_t *src_v = data_v_ + sy_in * stride_;
                uint8_t *dst_y = output.data_ + y * output.stride_;
                uint8_t *dst_u = output.data_u_ + y * output.stride_;
                uint8_t *dst_v = output.data_v_ + y * output.stride_;
                for (int32_t x = 0; x < output.width_; x++) {
                    // repeat edge pixels for out of bound area
                    int32_t sx_in = crop_left + ((ratio_w * x) >> 10);
                    sx_in = std::max<int32_t>(0, std::min<int32_t>(width_ - 1, sx_in));

                    dst_y[x] = src_y[sx_in];
                    dst_u[x] = src_u[sx_in];
                    dst_v[x] = src_v[sx_in];
                }
            }
        }
    } else if (format_ == FMT_BGR24 || format_ == FMT_RGB24 || format_ == FMT_BGRA32) {
        cv::Rect roi(crop_left, crop_top, crop_w, crop_h);
        int32_t vleft = std::max(0, crop_left);
        int32_t vtop = std::max(0, crop_top);
        int32_t vright = std::min(width_ - 1, crop_right);
        int32_t vbottom = std::min(height_ - 1, crop_bottom);
        cv::Rect inbound_roi(vleft, vtop, vright - vleft, vbottom - vtop);

        cv::Mat src = this->ToCVMat();
        if (roi != inbound_roi) {
            // fill out black to the out of bound region
            // in case of BGR, padding with repeating or reflection shows worst result
            cv::Mat cropped(crop_h, crop_w, matching_type, cv::Scalar(0));
            cv::Rect valid_roi_in_crop(0, 0, inbound_roi.width, inbound_roi.height);
            if (crop_left < 0)
                valid_roi_in_crop.x = -crop_left;
            if (crop_top < 0)
                valid_roi_in_crop.y = -crop_top;

            cv::Mat inbound_cropped(cropped, valid_roi_in_crop);
            src(inbound_roi).copyTo(inbound_cropped);
            cv::Mat resized(resize, matching_type, output.data_);
            cv::resize(cropped, resized, resize, 0, 0, cv::INTER_LINEAR_EXACT);
        } else {
            cv::Mat cropped = src(roi);
            cv::Mat resized(resize, matching_type, output.data_);
            cv::resize(cropped, resized, resize, 0, 0, cv::INTER_LINEAR_EXACT);
        }
    } else {
        ETHROW(false, logic_error, "Invalid container format for CropAndResize");
    }

    return output;
}

uint8_t *YuvImage::GetChannelPtr(int32_t idx) const {
    if (format_ == FMT_NV12) {
        if (idx == 0) {
            return data_;
        } else {
            if (uv_upsampled_) {
                if (idx == 1)
                    return data_u_;
                else
                    return data_v_;
            } else {
                return data_uv_;
            }
        }
    } else if (format_ == FMT_RGB24) {
        if (idx == 0)
            return data_;
        else if (idx == 1)
            return (data_ + 1);
        else
            return (data_ + 2);
    } else if (format_ == FMT_BGR24 || format_ == FMT_BGRA32) {
        if (idx == 2)
            return data_;
        else if (idx == 1)
            return (data_ + 1);
        else
            return (data_ + 2);
    }
    return nullptr;
}

cv::Mat YuvImage::ToCVMat() const {
    if (format_ == FMT_BGR24) {
        return cv::Mat(cv::Size(width_, height_), CV_8UC3, data_);
    } else if (format_ == FMT_BGRA32) {
        return cv::Mat(cv::Size(width_, height_), CV_8UC4, data_);
    } else if (format_ == FMT_NV12 || format_ == FMT_I420) {
        return cv::Mat(cv::Size(width_, height_ * 3 / 2), CV_8UC1, data_);
    } else {
        // convert gray mat
        return cv::Mat(cv::Size(width_, height_), CV_8UC1, data_);
    }
}

void YuvImage::Release() {
    if (is_reference_ == false && data_) {
        delete[] data_;
    }
    data_ = nullptr;
    data_uv_ = nullptr;
    data_u_ = nullptr;
    data_v_ = nullptr;
    size_ = 0;
    width_ = -1;
    height_ = -1;
    format_ = FMT_UNKNOWN;
    stride_ = 0;
    index_ = -1;
    uv_upsampled_ = false;
    is_reference_ = false;
}

}; // namespace ot
}; // namespace vas
