/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/memory_mapper.h"
#include "dlstreamer/openvino/tensor.h"
#include "dlstreamer/utils.h"

namespace dlstreamer {

class MemoryMapperCPUToOpenVINO : public BaseMemoryMapper {
  public:
    using BaseMemoryMapper::BaseMemoryMapper;

    TensorPtr map(TensorPtr src, AccessMode /*mode*/) override {
        auto &info = src->info();
        ov::Tensor tensor(data_type_to_openvino(info.dtype), info.shape, src->data(), info.stride);
        auto ret = std::make_shared<OpenVINOTensor>(tensor, _output_context);
        ret->set_parent(src);
        return ret;
    }

    FramePtr map(FramePtr src, AccessMode mode) override {
        TensorVector tensors;
        if (src->media_type() == MediaType::Tensors) {
            return BaseMemoryMapper::map(src, mode);
        } else if (src->media_type() == MediaType::Image) {
            auto tensor = src->tensor(0);
            if (src->format() != static_cast<int>(ImageFormat::I420)) {
                std::vector<size_t> shape = tensor->info().shape;
                auto ov_tensor = ov::Tensor(ov::element::u8, shape, tensor->data());
                tensors.push_back(std::make_shared<OpenVINOTensor>(ov_tensor, _output_context));
            }
#if 0
            ImageInfo info0(plane0->info());
            size_t w = info0.width();
            size_t h = info0.height();
            if (src->format() == static_cast<int>(ImageFormat::I420)) {
                auto plane1 = src->tensor(1);
                ImageInfo info1(plane1->info());
                auto plane2 = src->tensor(2);
                ImageInfo info2(plane2->info());
                IE::TensorDesc y_desc(IE::Precision::U8, {1, 1, h, info0.width_stride()}, IE::Layout::NHWC);
                IE::TensorDesc u_desc(IE::Precision::U8, {1, 1, h / 2, info1.width_stride()}, IE::Layout::NHWC);
                IE::TensorDesc v_desc(IE::Precision::U8, {1, 1, h / 2, info2.width_stride()}, IE::Layout::NHWC);
                IE::Blob::Ptr y_blob = IE::make_shared_blob<uint8_t>(y_desc, static_cast<uint8_t *>(plane0->data()));
                IE::Blob::Ptr u_blob = IE::make_shared_blob<uint8_t>(u_desc, static_cast<uint8_t *>(plane1->data()));
                IE::Blob::Ptr v_blob = IE::make_shared_blob<uint8_t>(u_desc, static_cast<uint8_t *>(plane2->data()));
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
                tensors.push_back(std::make_shared<OpenVINOTensor>(blob));
            }
#endif
            else {
                throw std::runtime_error("Unsupported color format " + std::to_string(src->format()));
            }
        } else {
            throw std::runtime_error("Unsupported media type");
        }
        auto ret = std::make_shared<BaseFrame>(MediaType::Tensors, 0, tensors);
        ret->set_parent(src);
        return ret;
    }
};

} // namespace dlstreamer
