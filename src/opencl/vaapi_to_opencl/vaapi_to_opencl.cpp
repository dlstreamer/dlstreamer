/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/opencl/elements/vaapi_to_opencl.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/dma/context.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/opencl/context.h"
#include "dlstreamer/vaapi/context.h"

namespace dlstreamer {

class VAAPIToOpenCL : public BaseTransform {
  public:
    VAAPIToOpenCL(DictionaryCPtr /*params*/, const ContextPtr &app_context) : BaseTransform(app_context) {
    }

    void set_input_info(const FrameInfo &info) override {
        if (!info.tensors.empty())
            _info = info;
    }

    void set_output_info(const FrameInfo &info) override {
        if (!info.tensors.empty())
            _info = info;
    }

    FrameInfoVector get_input_info() override {
        FrameInfo info1(MediaType::Tensors, MemoryType::VAAPI, _info.tensors);
        FrameInfo info2(ImageFormat::BGRX, MemoryType::VAAPI, _info.tensors);
        FrameInfo info3(ImageFormat::RGBX, MemoryType::VAAPI, _info.tensors);
        return {info1, info2, info3};
    }

    FrameInfoVector get_output_info() override {
        FrameInfo info1(MediaType::Tensors, MemoryType::OpenCL, _info.tensors);
        FrameInfo info2(MediaType::Tensors, MemoryType::OpenCL, _info.tensors);
        // HWC to NHWC
        for (auto &tinfo : info2.tensors) {
            while (tinfo.shape.size() < 4)
                tinfo.shape.insert(tinfo.shape.begin(), 1);
        }
        return {info1, info2};
    }

    FramePtr process(FramePtr src) override {
        DLS_CHECK(init());
        return _mapper->map(src, AccessMode::ReadWrite);
    }

    bool process(FramePtr /*src*/, FramePtr /*dst*/) override {
        throw std::runtime_error("Unsupported");
    }

    std::function<FramePtr()> get_output_allocator() override {
        return nullptr;
    }

  protected:
    bool init_once() override {
        auto vaapi_context = VAAPIContext::create(_app_context);
        auto dma_context = DMAContext::create(_app_context);
        auto opencl_context = OpenCLContext::create(_app_context);

        _mapper = create_mapper({_app_context, vaapi_context, dma_context, opencl_context});
        return true;
    }

  private:
    FrameInfo _info;
    MemoryMapperPtr _mapper;
};

extern "C" {
ElementDesc vaapi_to_opencl = {.name = "vaapi_to_opencl",
                               .description = "Convert memory:VASurface to memory:OpenCL",
                               .author = "Intel Corporation",
                               .params = nullptr,
                               .input_info = MAKE_FRAME_INFO_VECTOR(
                                   {{MediaType::Image, MemoryType::VAAPI}, {MediaType::Tensors, MemoryType::VAAPI}}),
                               .output_info = MAKE_FRAME_INFO_VECTOR({{MediaType::Tensors, MemoryType::OpenCL}}),
                               .create = create_element<VAAPIToOpenCL>,
                               .flags = 0};
}

} // namespace dlstreamer
