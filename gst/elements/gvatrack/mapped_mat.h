/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <dlstreamer/buffer_base.h>
#include <dlstreamer/fourcc.h>

#include <safe_arithmetic.hpp>

#include <opencv2/core/mat.hpp>

/**
 * @brief This class represents mapped data from GstBuffer in matrix form using cv::Mat
 */
class MappedMat {
  private:
    std::unique_ptr<uint8_t[]> data_storage;
    dlstreamer::BufferPtr buf;
    cv::Mat cv_mat;
    MappedMat();
    MappedMat(const MappedMat &);
    MappedMat &operator=(const MappedMat &);

    void copyPlanesToDataStorage(const dlstreamer::Buffer &buffer) {
        size_t total_size = 0;
        for (const auto &p : buffer.info()->planes) {
            total_size = safe_add(p.size(), total_size);
        }

        data_storage.reset(new uint8_t[total_size]);
        uint8_t *dst = data_storage.get();

        size_t offset = 0;
        for (size_t i = 0; i < buffer.info()->planes.size(); ++i) {
            const size_t size = buffer.info()->planes[i].size();
            std::memcpy(dst + offset, buffer.data(i), size);
            offset = safe_add(size, offset);
        }
    }

  public:
    /**
     * @brief Construct MappedMat instance from GstBuffer and GstVideoInfo
     * @param buffer buffer containing image of interest
     */
    MappedMat(dlstreamer::BufferPtr buffer) {
        if (!buffer)
            throw std::invalid_argument("GVA::MappedMat: Invalid buffer");
        assert(buffer->type() == dlstreamer::BufferType::CPU && "Buffer with system memory is expected");

        auto *data_ptr = static_cast<uint8_t *>(buffer->data(0));
        auto &info = *buffer->info();

        // Buffer can exceed the expected size due to decoder's padding. VAS OT expects a contiguous image with valid
        // data. Thus, repacking the image data to avoid undefined VAS OT behavior.
        if (info.planes.size() >= 2 && data_ptr + info.planes[0].size() != buffer->data(1)) {
            copyPlanesToDataStorage(*buffer);
            data_ptr = data_storage.get();
        } else {
            // Keep reference to the buffer
            buf = buffer;
        }

        cv::Size cv_size(info.planes.front().width(), info.planes.front().height());
        size_t stride = info.planes.front().width_stride();
        switch (info.format) {
        case dlstreamer::FourCC::FOURCC_BGR:
            cv_mat = cv::Mat(cv_size, CV_8UC3, data_ptr, stride);
            break;
        case dlstreamer::FourCC::FOURCC_NV12:
            cv_size.height *= 1.5;
            cv_mat = cv::Mat(cv_size, CV_8UC1, data_ptr, stride);
            break;
        case dlstreamer::FourCC::FOURCC_I420:
            cv_size.height *= 1.5;
            cv_mat = cv::Mat(cv_size, CV_8UC1, data_ptr, stride);
            break;
        case dlstreamer::FourCC::FOURCC_BGRX:
        case dlstreamer::FourCC::FOURCC_RGBX:
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
