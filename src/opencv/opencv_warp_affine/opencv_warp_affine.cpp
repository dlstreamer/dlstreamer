/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/opencv/elements/opencv_warp_affine.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/opencl/context.h"
#include "dlstreamer/opencl/tensor.h"
#include "dlstreamer/opencv_umat/context.h"
#include "dlstreamer/utils.h"
#include "dlstreamer/vaapi/context.h"
#include "dlstreamer/vaapi/frame_alloc.h"

namespace dlstreamer {

namespace param {
static constexpr auto angle = "angle";
static constexpr auto sync = "sync";
}; // namespace param

static ParamDescVector params_desc = {
    {param::angle, "Angle by which the picture is rotated (in radians)", 0.0, -1e10, 1e10},
    {param::sync, "Wait for OpenCL kernel completion (if running on GPU via cv::UMat)", false},
};

class OpenCvWarpAffine : public BaseTransform {
  public:
    OpenCvWarpAffine(DictionaryCPtr params, const ContextPtr &app_context) : BaseTransform(app_context) {
        _angle = params->get<double>(param::angle, 0);
        _sync = params->get<bool>(param::sync, false);
    }

    bool init_once() override {
        _vaapi_context = VAAPIContext::create(_app_context);
        _dma_context = DMAContext::create(_app_context);
        _umat_context = OpenCVUMatContext::create(_app_context);
        _opencl_context = OpenCLContext::create(_umat_context);

        create_mapper({_app_context, _vaapi_context, _dma_context, _opencl_context, _umat_context}, true);
        create_mapper({_dma_context, _opencl_context, _umat_context}, true);

        ImageInfo src_info(_input_info.tensors[0]);
        ImageInfo dst_info(_output_info.tensors[0]);
        _rot_mat = cv::getRotationMatrix2D(cv::Point2f(src_info.width() / 2, src_info.height() / 2), _angle, 1.0);
        _dst_size = cv::Size(dst_info.width(), dst_info.height());

        return true;
    }

    std::function<FramePtr()> get_output_allocator() override {
        return [this]() {
            FramePtr vaapi_frame = std::make_shared<VAAPIFrameAlloc>(_output_info, _vaapi_context);
            return vaapi_frame.map(_dma_context);
        };
    }

    bool process(TensorPtr src, TensorPtr dst) override {
        DLS_CHECK(init());
        cv::UMat src_umat = src.map<OpenCVUMatTensor>(_umat_context)->umat();
        cv::UMat dst_umat = dst.map<OpenCVUMatTensor>(_umat_context)->umat();

        cv::warpAffine(src_umat, dst_umat, _rot_mat, _dst_size);

        if (_sync && _umat_context)
            _umat_context->finish();

        return true;
    }

  private:
    VAAPIContextPtr _vaapi_context;
    DMAContextPtr _dma_context;
    OpenCVUMatContextPtr _umat_context;
    OpenCLContextPtr _opencl_context;
    double _angle = 0;
    bool _sync = false;
    cv::Mat _rot_mat;
    cv::Size _dst_size;
};

extern "C" {
ElementDesc opencv_warp_affine = {.name = "opencv_warp_affine",
                                  .description = "Rotation using cv::warpAffine",
                                  .author = "Intel Corporation",
                                  .params = &params_desc,
                                  .input_info = MAKE_FRAME_INFO_VECTOR({
                                      {ImageFormat::RGB, MemoryType::VAAPI},
                                      {ImageFormat::BGR, MemoryType::VAAPI},
                                      {ImageFormat::RGBX, MemoryType::VAAPI},
                                      {ImageFormat::BGRX, MemoryType::VAAPI},
                                  }),
                                  .output_info = MAKE_FRAME_INFO_VECTOR({
                                      {ImageFormat::RGB, MemoryType::DMA},
                                      {ImageFormat::BGR, MemoryType::DMA},
                                      {ImageFormat::RGBX, MemoryType::DMA},
                                      {ImageFormat::BGRX, MemoryType::DMA},
                                  }),
                                  .create = create_element<OpenCvWarpAffine>,
                                  .flags = 0};
}

} // namespace dlstreamer
