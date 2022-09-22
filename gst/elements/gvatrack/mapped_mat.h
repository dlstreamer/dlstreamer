/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <dlstreamer/base/frame.h>
#include <dlstreamer/image_info.h>

#include <safe_arithmetic.hpp>

#include <opencv2/core/mat.hpp>

/**
 * @brief This class represents mapped data from GstBuffer in matrix form using cv::Mat
 */
class MappedMat {
  private:
    std::unique_ptr<uint8_t[]> data_storage;
    dlstreamer::FramePtr buf;
    cv::Mat cv_mat;
    MappedMat();
    MappedMat(const MappedMat &);
    MappedMat &operator=(const MappedMat &);

    void copyPlanesToDataStorage(dlstreamer::Frame &buffer) {
        size_t total_size = 0;
        for (const auto &tensor : buffer) {
            total_size = safe_add(tensor->info().nbytes(), total_size);
        }

        data_storage.reset(new uint8_t[total_size]);
        uint8_t *dst = data_storage.get();

        size_t offset = 0;
        for (const auto &tensor : buffer) {
            const size_t size = tensor->info().nbytes();
            std::memcpy(dst + offset, tensor->data(), size);
            offset = safe_add(size, offset);
        }
    }

  public:
    /**
     * @brief Construct MappedMat instance from GstBuffer and GstVideoInfo
     * @param buffer buffer containing image of interest
     */
    MappedMat(dlstreamer::FramePtr buffer) {
        if (!buffer)
            throw std::invalid_argument("GVA::MappedMat: Invalid buffer");
        auto tensor0 = buffer->tensor(0);
        assert(tensor0->memory_type() == dlstreamer::MemoryType::CPU && "Buffer with system memory is expected");

        auto *data_ptr = static_cast<uint8_t *>(tensor0->data());
        auto &info0 = tensor0->info();

        // Buffer can exceed the expected size due to decoder's padding. VAS OT expects a contiguous image with valid
        // data. Thus, repacking the image data to avoid undefined VAS OT behavior.
        if (buffer->num_tensors() >= 2 && data_ptr + buffer->tensor(1)->info().nbytes() != buffer->tensor(1)->data()) {
            copyPlanesToDataStorage(*buffer);
            data_ptr = data_storage.get();
        } else {
            // Keep reference to the buffer
            buf = buffer;
        }

        dlstreamer::ImageInfo image_info(info0);
        cv::Size cv_size(image_info.width(), image_info.height());
        size_t stride = image_info.width_stride();
        switch (static_cast<dlstreamer::ImageFormat>(buffer->format())) {
        case dlstreamer::ImageFormat::BGR:
            cv_mat = cv::Mat(cv_size, CV_8UC3, data_ptr, stride);
            break;
        case dlstreamer::ImageFormat::NV12:
            cv_size.height *= 1.5;
            cv_mat = cv::Mat(cv_size, CV_8UC1, data_ptr, stride);
            break;
        case dlstreamer::ImageFormat::I420:
            cv_size.height *= 1.5;
            cv_mat = cv::Mat(cv_size, CV_8UC1, data_ptr, stride);
            break;
        case dlstreamer::ImageFormat::BGRX:
        case dlstreamer::ImageFormat::RGBX:
            cv_mat = cv::Mat(cv_size, CV_8UC4, data_ptr, stride);
            break;

        default:
            throw std::runtime_error("GVA::MappedMat: Unsupported format");
        }
    }

    ~MappedMat() {
    }

    /**
     * @brief Get mapped data from GstBuffer as a cv::Mat
     * @return data wrapped by cv::Mat
     */
    cv::Mat &mat() {
        return cv_mat;
    }
};
