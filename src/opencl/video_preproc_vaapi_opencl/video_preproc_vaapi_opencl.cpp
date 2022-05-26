/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "video_preproc_vaapi_opencl.h"
#include "dlstreamer/buffer_mappers/mapper_chain.h"
#include "dlstreamer/buffer_mappers/opencl_to_cpu.h"
#include "dlstreamer/buffer_mappers/opencl_to_dma.h"
#include "dlstreamer/opencl/buffer.h"
#include "dlstreamer/opencl/context.h"
#include "dlstreamer/opencl/utils.h"
#include "video_preproc_vaapi.h"

namespace dlstreamer {

namespace param {
static constexpr auto use_cl_image = "use_cl_image";
}; // namespace param

template <typename T>
static inline std::vector<T> append_vector(const std::vector<T> &v1, const std::vector<T> &v2) {
    std::vector<T> ret = v1;
    ret.insert(ret.end(), v2.begin(), v2.end());
    return ret;
}

static ParamDescVector params_desc = append_vector(
    *VideoPreprocVAAPIParamsDesc(), {{param::use_cl_image, "Allocate OpenCL memory as image (not buffer)", true}});

class VideoPreprocVAAPIOpenCL : public VideoPreprocVAAPI {
  public:
    VideoPreprocVAAPIOpenCL(ITransformController &transform_ctrl, DictionaryCPtr params)
        : VideoPreprocVAAPI(transform_ctrl, std::move(params)) {
        _use_cl_image = _params->get<bool>(param::use_cl_image);
        _desc = &VideoPreprocVAAPIOpenCLDesc;
    }

    void set_info(const BufferInfo &input_info, const BufferInfo &output_info) override {
        VideoPreprocVAAPI::set_info(input_info, output_info);

        _opencl_context = _transform_ctrl->get_context<OpenCLContext>();
        if (!_opencl_context)
            throw std::runtime_error("Can't query OpenCL context ");

        std::vector<BufferMapperPtr> chain = {std::make_shared<BufferMapperOpenCLToDMA>(),
                                              std::make_shared<BufferMapperDMAToVAAPI>(_vaapi_context)};
        _opencl_to_vaapi_mapper = std::make_shared<BufferMapperChain>(chain);
    }

    BufferInfoVector get_output_info(const BufferInfo &input_info) override {
        auto output_info = VideoPreprocVAAPI::get_output_info(input_info);

        // If OpenCL memory allocated as buffers, need to align strides according to VAAPI surface requirements
        if (/*!_use_cl_image &&*/ !output_info.empty() && !output_info.begin()->planes.empty()) {
            for (auto &plane : output_info.begin()->planes) {
                std::vector<size_t> shape = plane.shape;
                std::vector<size_t> stride(shape.size());
                Layout layout(shape);
                size_t size = plane.stride[plane.stride.size() - 1];
                for (int d = stride.size() - 1; d >= 0; d--) {
                    stride[d] = size;
                    if (d == layout.w_position())
                        size *= (shape[d] + (WIDTH_ALIGNMENT - 1)) & ~(WIDTH_ALIGNMENT - 1);
                    else if (d == layout.h_position())
                        size *= (shape[d] + (HEIGHT_ALIGNMENT - 1)) & ~(HEIGHT_ALIGNMENT - 1);
                    else
                        size *= shape[d];
                }
                plane = PlaneInfo(shape, plane.type, plane.name, stride);
            }
        }

        return output_info;
    }

    std::function<BufferPtr()> get_output_allocator() override {
        auto output_info_ptr = std::make_shared<BufferInfo>(get_output_info(_input_info)[0]);
        return [this, output_info_ptr]() {
            std::vector<cl_mem> mems(output_info_ptr->planes.size());
            for (size_t i = 0; i < mems.size(); i++) {
                auto &plane = output_info_ptr->planes[i];
                cl_int errcode = 0;
                if (_use_cl_image) {
                    cl_image_format format = {};
                    format.image_channel_data_type = data_type_to_opencl(plane.type);
                    format.image_channel_order = num_channels_to_opencl(plane.channels());

                    _cl_image_desc desc = {};
                    desc.image_type = CL_MEM_OBJECT_IMAGE2D;
                    desc.image_height = plane.height() * plane.batch();
                    desc.image_width = plane.width();
                    desc.image_array_size = 1;
                    mems[i] = clCreateImage(_opencl_context->context(), 0, &format, &desc, NULL, 0);
                } else {
                    mems[i] = clCreateBuffer(_opencl_context->context(), 0, plane.size(), 0, &errcode);
                }
                if (!mems[i])
                    throw std::runtime_error("Error creating OpenCL memory: " + std::to_string(errcode));
            }
            auto ret = std::make_shared<OpenCLBufferRefCounted>(output_info_ptr, _opencl_context, mems);
            for (auto mem : mems) {
                clReleaseMemObject(mem);
            }
            return ret;
        };
    }

    BufferMapperPtr get_output_mapper() override {
        return std::make_shared<BufferMapperOpenCLToCPU>();
    }

  protected:
    VAAPIBufferPtr dst_buffer_to_vaapi(BufferPtr dst) override {
        return _opencl_to_vaapi_mapper->map<VAAPIBuffer>(dst, AccessMode::WRITE);
    }

  private:
    bool _use_cl_image = false;
    BufferMapperPtr _opencl_to_vaapi_mapper;
    OpenCLContextPtr _opencl_context;
    static constexpr int WIDTH_ALIGNMENT = 32;
    static constexpr int HEIGHT_ALIGNMENT = 32;
};

TransformDesc VideoPreprocVAAPIOpenCLDesc = {
    .name = "video_preproc_vaapi_opencl",
    .description = "Batched pre-processing with VAAPI memory input and OpenCL memory output",
    .author = "Intel Corporation",
    .params = &params_desc,
    .input_info = {{FourCC::FOURCC_BGRX, BufferType::VAAPI_SURFACE}},
    .output_info = {{MediaType::TENSORS, BufferType::OPENCL_BUFFER}},
    .create = TransformBase::create<VideoPreprocVAAPIOpenCL>,
    .flags = TRANSFORM_FLAG_OUTPUT_ALLOCATOR | TRANSFORM_FLAG_SHARABLE | TRANSFORM_FLAG_MULTISTREAM_MUXER};

} // namespace dlstreamer
