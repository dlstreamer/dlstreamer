/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_backend/logger.h"
#include "inference_backend/pre_proc.h"

#include <opencv2/opencv.hpp>

namespace InferenceBackend {

class OpenCV_VPP : public PreProc {
  public:
    OpenCV_VPP() {
    }

    virtual ~OpenCV_VPP() {
    }

    void Convert(const Image &src, Image &dst, bool bAllocateDestination) {
        ITT_TASK("OpenCV_VPP");

        if (bAllocateDestination) {
            throw std::runtime_error("OpenCV_VPP: bAllocateDestination==true not supported");
        }

        if (src.format == dst.format && src.width == dst.width && src.height == dst.height) {
            // TODO: expected only RGBP here
            assert(src.format == FourCC::FOURCC_RGBP);

            int planes_count = GetPlanesCount(src.format);
            for (int i = 0; i < planes_count; i++) {
                if (src.width == src.stride[i]) {
                    memcpy(dst.planes[i], src.planes[i], src.width * src.height * sizeof(uint8_t));
                } else {
                    int dst_stride = src.width * sizeof(uint8_t);
                    int src_stride = src.stride[i] * sizeof(uint8_t);
                    for (int r = 0; r < src.height; r++) {
                        memcpy(dst.planes[i] + r * dst_stride, src.planes[i] + r * src_stride, dst_stride);
                    }
                }
            }
        }

        cv::Mat mat;
        switch (src.format) {
        case FOURCC_BGRX:
        case FOURCC_BGRA:
            mat = cv::Mat(src.height, src.width, CV_8UC4, src.planes[0], src.stride[0]);
            break;
        case FOURCC_BGR:
            mat = cv::Mat(src.height, src.width, CV_8UC3, src.planes[0], src.stride[0]);
            break;
        case FOURCC_RGBP: {
            cv::Mat channelR = cv::Mat(src.height, src.width, CV_8UC1, src.planes[0], src.stride[0]);
            cv::Mat channelG = cv::Mat(src.height, src.width, CV_8UC1, src.planes[1], src.stride[1]);
            cv::Mat channelB = cv::Mat(src.height, src.width, CV_8UC1, src.planes[2], src.stride[2]);
            std::vector<cv::Mat> channels{channelB, channelG, channelR};
            cv::merge(channels, mat);
            break;
        }
        default:
            throw std::runtime_error("OpenCV_VPP: Unsupported format");
        }

        switch (dst.format) {
        case FOURCC_RGBP:
            ResizeRGB2RGBP<uint8_t>(mat, dst);
            break;
        default:
            throw std::runtime_error("OpenCV_VPP: Can not convert to desired format. Not implemented");
        }
    }

    virtual void ReleaseImage(const Image &) {
    }

  protected:
    template <typename T>
    static void ResizeRGB2RGBP(const cv::Mat &orig_image, const Image &dst) {
        ITT_TASK(__FUNCTION__);
        const size_t width = dst.width;
        const size_t height = dst.height;
        const size_t channels = 3;
        uint8_t *dst_data = dst.planes[0];

        cv::Mat resized_image(orig_image);
        if (width != (size_t)orig_image.size().width || height != (size_t)orig_image.size().height) {
            ITT_TASK("cv::resize");
            cv::resize(orig_image, resized_image, cv::Size(width, height));
        }

        if (resized_image.channels() == 4) {
            ITT_TASK("RGB32->RGBP");
            for (size_t c = 0; c < channels; c++) {
                for (size_t h = 0; h < height; h++) {
                    for (size_t w = 0; w < width; w++) {
                        dst_data[c * width * height + h * width + w] = resized_image.at<cv::Vec4b>(h, w)[c];
                    }
                }
            }
        } else if (resized_image.channels() == 3) {
            ITT_TASK("RGB24->RGBP");
            for (size_t c = 0; c < channels; c++) {
                for (size_t h = 0; h < height; h++) {
                    for (size_t w = 0; w < width; w++) {
                        dst_data[c * width * height + h * width + w] = resized_image.at<cv::Vec3b>(h, w)[c];
                    }
                }
            }
        } else {
            throw std::runtime_error("Image with unsupported number channels");
        }
    }
};

PreProc *CreatePreProcOpenCV() {
    return new OpenCV_VPP();
}
} // namespace InferenceBackend
