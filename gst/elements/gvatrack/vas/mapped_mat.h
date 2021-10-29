/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "buffer_map/buffer_mapper.h"
#include "utils.h" // GetPlanesCount

#include <safe_arithmetic.hpp>

#include <opencv2/core/mat.hpp>

/**
 * @brief This class represents mapped data from GstBuffer in matrix form using cv::Mat
 */
class MappedMat {
  private:
    BufferMapper &buffer_mapper;
    InferenceBackend::Image image;
    std::unique_ptr<uint8_t[]> data_storage;
    cv::Mat cv_mat;
    MappedMat();
    MappedMat(const MappedMat &);
    MappedMat &operator=(const MappedMat &);

    void copyPlanesToDataStorage(const InferenceBackend::Image &image, const std::vector<size_t> &planes_size) {
        size_t total_size = 0;
        for (const size_t &n : planes_size) {
            total_size = safe_add(n, total_size);
        }

        data_storage.reset(new uint8_t[total_size]);
        uint8_t *dst = data_storage.get();

        size_t offset = 0;
        for (size_t i = 0; i < planes_size.size(); ++i) {
            std::memcpy(dst + offset, image.planes[i], planes_size[i]);
            offset = safe_add(planes_size[i], offset);
        }
    }

  public:
    /**
     * @brief Construct MappedMat instance from GstBuffer and GstVideoInfo
     * @param buffer GstBuffer* containing image of interest
     * @param video_info GstVideoInfo* containing video information
     * @param flag GstMapFlags flags used when mapping memory
     */
    MappedMat(GstBuffer *buffer, BufferMapper &buf_mapper, GstMapFlags flag = GST_MAP_READ)
        : buffer_mapper(buf_mapper) {
        if (!buffer)
            throw std::invalid_argument("GVA::MappedMat: Invalid buffer");
        assert(buffer_mapper.memoryType() == InferenceBackend::MemoryType::SYSTEM &&
               "Mapper to system memory is expected");

        image = buffer_mapper.map(buffer, flag);

        auto format = image.format;
        size_t width = image.width, height = image.height;
        size_t stride = image.stride[0];

        uint8_t *data_ptr = reinterpret_cast<uint8_t *>(image.planes[0]);

        // Buffer can exceed the expected size due to decoder's padding. VAS OT expects a contiguous image with valid
        // data. Thus, repacking the image data to avoid undefined VAS OT behavior.
        if (Utils::GetPlanesCount(format) >= 2 && image.planes[1] > image.planes[0] + stride * height) {
            std::vector<size_t> planes_sizes;
            const auto area = safe_mul<size_t>(width, height);
            if (format == InferenceBackend::FourCC::FOURCC_I420) {
                planes_sizes = std::vector<size_t>{area, area / 4, area / 4};
            } else if (format == InferenceBackend::FourCC::FOURCC_NV12) {
                planes_sizes = std::vector<size_t>{area, area / 2};
            } else {
                throw std::runtime_error("GVA::MappedMat: Unsupported format!");
            }
            copyPlanesToDataStorage(image, planes_sizes);
            data_ptr = data_storage.get();
        }

        switch (format) {
        case InferenceBackend::FourCC::FOURCC_BGR:
            cv_mat = cv::Mat(cv::Size(width, height), CV_8UC3, data_ptr, stride);
            break;
        case InferenceBackend::FourCC::FOURCC_NV12:
            cv_mat = cv::Mat(cv::Size(width, height * 3 / 2), CV_8UC1, data_ptr, stride);
            break;
        case InferenceBackend::FourCC::FOURCC_I420:
            cv_mat = cv::Mat(cv::Size(width, height * 3 / 2), CV_8UC1, data_ptr, stride);
            break;
        case InferenceBackend::FourCC::FOURCC_BGRA:
        case InferenceBackend::FourCC::FOURCC_BGRX:
            cv_mat = cv::Mat(cv::Size(width, height), CV_8UC4, data_ptr, stride);
            break;

        default:
            throw std::runtime_error("GVA::MappedMat: Unsupported format");
        }
    }

    ~MappedMat() {
        buffer_mapper.unmap(image);
    }

    /**
     * @brief Get mapped data from GstBuffer as a cv::Mat
     * @return data wrapped by cv::Mat
     */
    cv::Mat &mat() {
        return cv_mat;
    }
};
