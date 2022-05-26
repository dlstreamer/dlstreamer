/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"

#include "dlstreamer/buffer_mapper.h"
#include "dlstreamer/openvino/buffer.h"
#include "dlstreamer/utils.h"

namespace dlstreamer {

class BufferMapperCPUToOpenVINO : public BufferMapper {
  public:
    BufferPtr map(BufferPtr src_buffer, AccessMode mode) override {
        auto buffer = std::dynamic_pointer_cast<CPUBuffer>(src_buffer);
        if (!buffer)
            throw std::runtime_error("Failed to dynamically cast Buffer to CPUBuffer");
        return map(buffer, mode);
    }

    OpenVINOBlobsBufferPtr map(CPUBufferPtr buffer, AccessMode /*mode*/) {
        auto info = buffer->info();
        OpenVINOBlobsBuffer::Blobs blobs;

        if (info->media_type == MediaType::VIDEO) {
            size_t w = info->planes[0].width();
            size_t h = info->planes[0].height();
            if (info->format == FOURCC_I420) {
                IE::TensorDesc y_desc(IE::Precision::U8, {1, 1, h, info->planes[0].width_stride()}, IE::Layout::NHWC);
                IE::TensorDesc u_desc(IE::Precision::U8, {1, 1, h / 2, info->planes[1].width_stride()},
                                      IE::Layout::NHWC);
                IE::TensorDesc v_desc(IE::Precision::U8, {1, 1, h / 2, info->planes[2].width_stride()},
                                      IE::Layout::NHWC);
                IE::Blob::Ptr y_blob = IE::make_shared_blob<uint8_t>(y_desc, static_cast<uint8_t *>(buffer->data(0)));
                IE::Blob::Ptr u_blob = IE::make_shared_blob<uint8_t>(u_desc, static_cast<uint8_t *>(buffer->data(1)));
                IE::Blob::Ptr v_blob = IE::make_shared_blob<uint8_t>(u_desc, static_cast<uint8_t *>(buffer->data(2)));
                size_t roi_x, roi_y, roi_w, roi_h;
#if 0
               if (roi_ptr) {
                    float xmin = std::max(0.f, roi_ptr->xmin);
                    float ymin = std::max(0.f, roi_ptr->ymin);
                    float xmax = std::min(1.f, roi_ptr->xmax);
                    float ymax = std::min(1.f, roi_ptr->ymax);
                    roi_x = static_cast<size_t>(xmin * w);
                    roi_y = static_cast<size_t>(ymin * h);
                    roi_w = static_cast<size_t>((xmax - xmin) * w);
                    roi_h = static_cast<size_t>((ymax - ymin) * h);
                } else
#endif
                {
                    roi_x = 0;
                    roi_y = 0;
                    roi_w = w;
                    roi_h = h;
                }
                IE::ROI roi_y_plane = {
                    0,
                    (roi_x & 0x1) ? roi_x - 1 : roi_x,
                    (roi_y & 0x1) ? roi_y - 1 : roi_y,
                    (roi_w & 0x1) ? roi_w - 1 : roi_w,
                    (roi_h & 0x1) ? roi_h - 1 : roi_h,
                };
                IE::ROI roi_uv_plane({0, roi_x / 2, roi_y / 2, roi_w / 2, roi_h / 2});
                IE::Blob::Ptr y_blob_roi = make_shared_blob(y_blob, roi_y_plane);
                IE::Blob::Ptr u_blob_roi = make_shared_blob(u_blob, roi_uv_plane);
                IE::Blob::Ptr v_blob_roi = make_shared_blob(v_blob, roi_uv_plane);
                auto blob = IE::make_shared_blob<IE::I420Blob>(y_blob_roi, u_blob_roi, v_blob_roi);
                blobs.push_back(blob);
            } else {
                throw std::runtime_error("Unsupported color format " + std::to_string(info->format));
            }
        } else if (info->media_type == MediaType::TENSORS) {
            for (size_t i = 0; i < info->planes.size(); i++) {
                auto desc = plane_info_to_tensor_desc(info->planes[i]);
                auto data = buffer->data(i);
                IE::Blob::Ptr blob;
                if (info->planes[i].type == DataType::U8)
                    blob = IE::make_shared_blob<uint8_t>(desc, static_cast<uint8_t *>(data));
                else if (info->planes[i].type == DataType::FP32)
                    blob = IE::make_shared_blob<float>(desc, static_cast<float *>(data));
                else if (info->planes[i].type == DataType::I32)
                    blob = IE::make_shared_blob<int32_t>(desc, static_cast<int32_t *>(data));
                else
                    throw std::runtime_error("Unsupported data type " + datatype_to_string(info->planes[i].type));
                // TODO OpenVINO™ toolkit API doesn't support strides
                blobs.push_back(blob);
            }
        }
        return OpenVINOBlobsBufferPtr(new OpenVINOBlobsBuffer(blobs),
                                      [buffer](OpenVINOBlobsBuffer *dst) { delete dst; });
    }
};

#ifdef HAVE_OPENVINO2
class BufferMapperCPUToOpenVINO2 : public BufferMapper {
  public:
    BufferPtr map(BufferPtr src_buffer, AccessMode mode) override {
        auto buffer = std::dynamic_pointer_cast<CPUBuffer>(src_buffer);
        if (!buffer)
            throw std::runtime_error("Failed to dynamically cast Buffer to CPUBuffer");
        return map(std::move(buffer), mode);
    }

    OpenVinoTensorsBufferPtr map(CPUBufferPtr buffer, AccessMode /*mode*/) {
        auto info = buffer->info();

        // FIXME: add support for video/audio?
        if (info->media_type != MediaType::TENSORS)
            throw std::runtime_error("Unsupported media type to map: " +
                                     std::to_string(static_cast<int>(info->media_type)));

        ov::runtime::TensorVector tensors;
        tensors.reserve(info->planes.size());
        for (size_t i = 0; i < info->planes.size(); i++) {
            const auto &plane = info->planes[i];
            // FIXME: pass strides?
            tensors.emplace_back(ov::element::u8, plane.shape, buffer->data(i));
        }

        // Keep reference to original buffer, so that memory stay valid
        // FIXME: find better approach. We repeat this in every mapper.
        return OpenVinoTensorsBufferPtr(new OpenVinoTensorsBuffer(std::move(tensors), {}),
                                        [buffer](OpenVinoTensorsBuffer *buf) { delete buf; });
    }
};
#endif

} // namespace dlstreamer
