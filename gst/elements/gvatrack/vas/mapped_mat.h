/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef VA_GSTREAMER_PLUGINS_MAPPED_MAT_H
#define VA_GSTREAMER_PLUGINS_MAPPED_MAT_H

#include "gva_buffer_map.h"
#include <opencv2/core/mat.hpp>

/**
 * @brief This class represents mapped data from GstBuffer in matrix form using cv::Mat
 */
class MappedMat {
  private:
    BufferMapContext map_context;
    std::unique_ptr<uint8_t[]> data_storage;
    cv::Mat cv_mat;
    MappedMat();
    MappedMat(const MappedMat &);
    MappedMat &operator=(const MappedMat &);

    void copyPlanesToDataStorage(const InferenceBackend::Image &image, const std::vector<size_t> &planes_size) {
        size_t total_size = 0;
        for (const size_t &n : planes_size) {
            total_size += n;
        }

        data_storage.reset(new uint8_t[total_size]);
        uint8_t *dst = data_storage.get();

        size_t offset = 0;
        for (size_t i = 0; i < planes_size.size(); ++i) {
            std::memcpy(dst + offset, image.planes[i], planes_size[i]);
            offset += planes_size[i];
        }
    }

  public:
    /**
     * @brief Construct MappedMat instance from GstBuffer and GstVideoInfo
     * @param buffer GstBuffer* containing image of interest
     * @param video_info GstVideoInfo* containing video information
     * @param flag GstMapFlags flags used when mapping memory
     */
    MappedMat(GstBuffer *buffer, GstVideoInfo *video_info, GstMapFlags flag = GST_MAP_READ) {
        InferenceBackend::Image image;
        gva_buffer_map(buffer, image, map_context, video_info, InferenceBackend::MemoryType::SYSTEM, flag);

        GstVideoFormat format = video_info->finfo->format;
        size_t width = video_info->width, height = video_info->height;
        size_t stride = video_info->stride[0];

        uint8_t *data_ptr = static_cast<uint8_t *>(map_context.frame.data[0]);

        // Buffer can exceed the expected size due to decoder's padding. VAS OT expects a contiguous image with valid
        // data. Thus, repacking the image data to avoid undefined VAS OT behavior.
        if (image.planes[1] > image.planes[0] + stride * height) {
            std::vector<size_t> planes_sizes;
            if (format == GST_VIDEO_FORMAT_I420) {
                planes_sizes = std::vector<size_t>{width * height, width * height / 4, width * height / 4};
            } else if (format == GST_VIDEO_FORMAT_NV12) {
                planes_sizes = std::vector<size_t>{width * height, width * height / 2};
            } else {
                throw std::runtime_error("GVA::MappedMat: Unsupported format!");
            }
            copyPlanesToDataStorage(image, planes_sizes);
            data_ptr = data_storage.get();
        }

        switch (format) {
        case GST_VIDEO_FORMAT_BGR:
            cv_mat = cv::Mat(cv::Size(video_info->width, video_info->height), CV_8UC3, data_ptr, stride);
            break;
        case GST_VIDEO_FORMAT_NV12:
            cv_mat = cv::Mat(cv::Size(video_info->width, video_info->height * 3 / 2), CV_8UC1, data_ptr, stride);
            break;
        case GST_VIDEO_FORMAT_I420:
            cv_mat = cv::Mat(cv::Size(video_info->width, video_info->height * 3 / 2), CV_8UC1, data_ptr, stride);
            break;
        case GST_VIDEO_FORMAT_BGRA:
        case GST_VIDEO_FORMAT_BGRx:
            cv_mat = cv::Mat(cv::Size(video_info->width, video_info->height), CV_8UC4, data_ptr, stride);
            break;

        default:
            throw std::runtime_error("GVA::MappedMat: Unsupported format");
        }
    }

    ~MappedMat() {
        gva_buffer_unmap(map_context);
    }

    /**
     * @brief Get mapped data from GstBuffer as a cv::Mat
     * @return data wrapped by cv::Mat
     */
    cv::Mat &mat() {
        return cv_mat;
    }
};

#endif // VA_GSTREAMER_PLUGINS_MAPPED_MAT_H
