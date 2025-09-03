/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/opencv/elements/opencv_cropscale.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/cpu/context.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/opencv/context.h"
#include "dlstreamer/opencv/mappers/cpu_to_opencv.h"
#include "dlstreamer/opencv/tensor.h"
#include "dlstreamer/utils.h"

namespace dlstreamer {

namespace param {

static constexpr auto add_borders = "add-borders"; // aspect-ratio

}; // namespace param

static ParamDescVector params_desc = {
    {param::add_borders, "Add borders if necessary to keep the aspect ratio", false},
};

class OpencvCropscale : public BaseTransform {
  public:
    OpencvCropscale(DictionaryCPtr params, const ContextPtr &app_context) : BaseTransform(app_context) {
        _aspect_ratio = params->get<bool>(param::add_borders, false);
    }

    FrameInfoVector get_input_info() override {
        if (_output_info.tensors.empty()) {
            return opencv_cropscale.input_info();
        } else {
            FrameInfo info(static_cast<ImageFormat>(_output_info.format), _output_info.memory_type); // any image size
            return {_output_info, info};
        }
    }

    FrameInfoVector get_output_info() override {
        if (_input_info.tensors.empty()) {
            return opencv_cropscale.output_info();
        } else {
            FrameInfo info(static_cast<ImageFormat>(_input_info.format), _input_info.memory_type); // any image size
            return {_input_info, info};
        }
    }

    bool init_once() override {
        auto cpu_context = std::make_shared<CPUContext>();
        auto opencv_context = std::make_shared<OpenCVContext>();
        _opencv_mapper = create_mapper({_app_context, cpu_context, opencv_context});
        return true;
    }

    bool process(FramePtr src, FramePtr dst) override {
        DLS_CHECK(init());
        auto src_tensor = ptr_cast<OpenCVTensor>(_opencv_mapper->map(src->tensor(), AccessMode::Read));
        auto dst_tensor = ptr_cast<OpenCVTensor>(_opencv_mapper->map(dst->tensor(), AccessMode::Write));
        cv::Mat src_mat = *src_tensor;
        cv::Mat dst_mat = *dst_tensor;

        if (!src_mat.cols || !src_mat.rows || !dst_mat.cols || !dst_mat.rows)
            throw std::runtime_error("Invalid OpenCV matrix");
        double src_w = src_mat.cols;
        double src_h = src_mat.rows;
        double dst_w = dst_mat.cols;
        double dst_h = dst_mat.rows;
        cv::Rect src_rect = {0, 0, src_mat.cols, src_mat.rows};
        cv::Rect dst_rect = {0, 0, dst_mat.cols, dst_mat.rows};

        if (_aspect_ratio) {
            double scale_x = static_cast<double>(dst_rect.width) / src_rect.width;
            double scale_y = static_cast<double>(dst_rect.height) / src_rect.height;
            double scale = std::min(scale_x, scale_y);
            dst_rect.width = src_rect.width * scale;
            dst_rect.height = src_rect.height * scale;
        }

        cv::resize(src_mat(src_rect), dst_mat(dst_rect), {dst_rect.width, dst_rect.height});

        // Store metadata with coefficients for src<>dst coordinates conversion
        auto affine_meta = dst->metadata().add(AffineTransformInfoMetadata::name);
        AffineTransformInfoMetadata(affine_meta).set_rect(src_w, src_h, dst_w, dst_h, src_rect, dst_rect);

        return true;
    }

    std::function<FramePtr()> get_output_allocator() override {
        return nullptr;
    }

  private:
    MemoryMapperPtr _opencv_mapper;
    bool _aspect_ratio = false;
};

extern "C" {
ElementDesc opencv_cropscale = {.name = "opencv_cropscale",
                                .description = "Fused video crop and scale on OpenCV backend. "
                                               "Crop operation supports GstVideoCropMeta if attached to input buffer",
                                .author = "Intel Corporation",
                                .params = &params_desc,
                                .input_info = MAKE_FRAME_INFO_VECTOR({
                                    //{ImageFormat::I420},
                                    //{ImageFormat::NV12},
                                    {ImageFormat::RGB},
                                    {ImageFormat::BGR},
                                    {ImageFormat::RGBX},
                                    {ImageFormat::BGRX},
                                    //{ImageFormat::RGBP},
                                    //{ImageFormat::BGRP},
                                }),
                                .output_info = MAKE_FRAME_INFO_VECTOR({
                                    //{ImageFormat::I420},
                                    //{ImageFormat::NV12},
                                    {ImageFormat::RGB},
                                    {ImageFormat::BGR},
                                    {ImageFormat::RGBX},
                                    {ImageFormat::BGRX},
                                    //{ImageFormat::RGBP},
                                    //{ImageFormat::BGRP}
                                }),
                                .create = create_element<OpencvCropscale>,
                                .flags = ELEMENT_FLAG_EXTERNAL_MEMORY};
}

} // namespace dlstreamer
