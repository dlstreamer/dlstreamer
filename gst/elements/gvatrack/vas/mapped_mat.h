/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef VA_GSTREAMER_PLUGINS_MAPPED_MAT_H
#define VA_GSTREAMER_PLUGINS_MAPPED_MAT_H

#include <opencv2/core.hpp>

/**
 * @brief This class represents mapped data from GstBuffer in matrix form using cv::Mat
 */
class MappedMat {
  private:
    GstBuffer *buffer;
    GstMapInfo map_info;
    cv::Mat cv_mat;

    MappedMat();
    MappedMat(const MappedMat &);
    MappedMat &operator=(const MappedMat &);

  public:
    /**
     * @brief Construct MappedMat instance from GstBuffer and GstVideoInfo
     * @param buffer GstBuffer* containing image of interest
     * @param video_info GstVideoInfo* containing video information
     * @param flag GstMapFlags flags used when mapping memory
     */
    MappedMat(GstBuffer *buffer, const GstVideoInfo *video_info, GstMapFlags flag = GST_MAP_READ) : buffer(buffer) {
        if (!gst_buffer_map(buffer, &map_info, flag))
            throw std::runtime_error("GVA::MappedMat: Could not map buffer to system memory");

        GstVideoFormat format = video_info->finfo->format;
        int stride = video_info->stride[0];

        switch (format) {
        case GST_VIDEO_FORMAT_BGR:
            this->mat() = cv::Mat(cv::Size(video_info->width, video_info->height), CV_8UC3,
                                  reinterpret_cast<char *>(map_info.data), stride);
            break;
        case GST_VIDEO_FORMAT_NV12:
            this->mat() = cv::Mat(cv::Size(video_info->width, static_cast<int32_t>(video_info->height * 1.5f)), CV_8UC1,
                                  reinterpret_cast<char *>(map_info.data), stride);
            break;
        case GST_VIDEO_FORMAT_BGRA:
        case GST_VIDEO_FORMAT_BGRx:
            this->mat() = cv::Mat(cv::Size(video_info->width, video_info->height), CV_8UC4,
                                  reinterpret_cast<char *>(map_info.data), stride);
            break;

        default:
            throw std::runtime_error("GVA::MappedMat: Unsupported format");
        }
    }

    ~MappedMat() {
        if (buffer != nullptr)
            gst_buffer_unmap(buffer, &map_info);
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
