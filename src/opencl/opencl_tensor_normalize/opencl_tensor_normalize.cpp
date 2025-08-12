/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/opencl/elements/opencl_tensor_normalize.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/opencl/context.h"

namespace dlstreamer {

#define INPUT_DTYPE DataType::UInt8
#define INPUT_TYPE "unsigned char"
#define OUTPUT_DTYPE DataType::Float32
#define OUTPUT_TYPE "float"

// The macro assumes _ERR variable used inside _FUNC to capture error code
#define CL_CALL(_FUNC)                                                                                                 \
    {                                                                                                                  \
        /*ITT_TASK(#_FUNC);*/                                                                                          \
        cl_int _ERR = 0;                                                                                               \
        _FUNC;                                                                                                         \
        if (_ERR) {                                                                                                    \
            throw std::runtime_error(#_FUNC " failed, err=" + std::to_string(_ERR));                                   \
        }                                                                                                              \
    }

static const char *OPENCL_PROGRAM = " \
__kernel void NHWC_TO_NCHW(__global char* src, __global char* dst, \
            const int src_stride0, const int src_stride1, const int src_stride2, const int src_stride3, \
            const int dst_stride0, const int dst_stride1, const int dst_stride2, const int dst_stride3) \
{ \
    /* Indexes for first three dimensions 'NHW', assuming dimension 'C' is equal 4 for src and 3 for dst */ \
    int i0 = get_global_id(0); \
    int i1 = get_global_id(1); \
    int i2 = get_global_id(2); \
\
    int src_offset = i0 * src_stride0 + i1 * src_stride1 + i2 * src_stride2; \
    int dst_offset = i0 * dst_stride0 + i1 * dst_stride2 + i2 * dst_stride3; \
\
    *(OUTPUT_TYPE*)(dst + dst_offset) = *(INPUT_TYPE*)(src + src_offset); \
    *(OUTPUT_TYPE*)(dst + dst_offset + dst_stride1) = *(INPUT_TYPE*)(src + src_offset + src_stride3); \
    *(OUTPUT_TYPE*)(dst + dst_offset + 2*dst_stride1) = *(INPUT_TYPE*)(src + src_offset + 2*src_stride3); \
} \
";

class OpenclTensorNormalize : public BaseTransform {
  public:
    OpenclTensorNormalize(DictionaryCPtr /*params*/, const ContextPtr &app_context) : BaseTransform(app_context) {
    }

    FrameInfoVector get_input_info() override {
        if (_output_info.tensors.empty()) {
            return opencl_tensor_normalize.input_info;
        } else {
            FrameInfo input_info = _output_info;
            // Move C dimension to last dimension
            for (auto &tinfo : input_info.tensors) {
                std::vector<size_t> shape = tinfo.shape;
                size_t c_position = ImageLayout(shape).c_position();
                if (shape[c_position] == 3) // RGB to RGBx
                    shape[c_position] = 4;
                vector_move_element(shape, c_position, shape.size() - 1);
                tinfo = TensorInfo(std::move(shape), INPUT_DTYPE);
            }
            return {input_info};
        }
    }

    FrameInfoVector get_output_info() override {
        if (_input_info.tensors.empty()) {
            return opencl_tensor_normalize.output_info;
        } else {
            FrameInfo output_info = _input_info;
            // Move C dimension before HW
            for (auto &tinfo : output_info.tensors) {
                std::vector<size_t> shape = tinfo.shape;
                size_t c_position = ImageLayout(shape).c_position();
                if (shape[c_position] == 4) // RGBx to RGB
                    shape[c_position] = 3;
                vector_move_element(shape, c_position, shape.size() - 3);
                tinfo = TensorInfo(std::move(shape), OUTPUT_DTYPE);
            }
            return {output_info};
        }
    }

    bool init_once() override {
        _opencl_context = OpenCLContext::create(_app_context);
        auto cl_ctx = _opencl_context->context();

        create_mapper({_app_context, _opencl_context});

        // Create command queue on first device in context
        size_t num_devices = 0;
        CL_CALL(_ERR = clGetContextInfo(cl_ctx, CL_CONTEXT_DEVICES, 0, NULL, &num_devices));
        if (!num_devices)
            throw std::runtime_error("OpenCL context contains no devices");
        std::vector<cl_device_id> devices(num_devices);
        clGetContextInfo(cl_ctx, CL_CONTEXT_DEVICES, num_devices, devices.data(), NULL);
        cl_queue_properties properties = 0;
        CL_CALL(_queue = clCreateCommandQueueWithProperties(cl_ctx, devices[0], &properties, &_ERR));

        // Compile OpenCL kernel
        std::string program_source = OPENCL_PROGRAM;
        string_find_and_replace(program_source, "INPUT_TYPE", std::string("__global ") + INPUT_TYPE);
        string_find_and_replace(program_source, "OUTPUT_TYPE", std::string("__global ") + OUTPUT_TYPE);
        const char *program_source_ptr = program_source.c_str();
        CL_CALL(_program = clCreateProgramWithSource(cl_ctx, 1, &program_source_ptr, NULL, &_ERR));
        auto status = clBuildProgram(_program, 1, devices.data(), NULL, NULL, NULL);
        if (status != CL_SUCCESS) {
            size_t log_size;
            clGetProgramBuildInfo(_program, devices[0], CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size);
            std::vector<char> build_log(log_size);
            clGetProgramBuildInfo(_program, devices[0], CL_PROGRAM_BUILD_LOG, log_size, &build_log[0], nullptr);
            throw std::runtime_error("Error building OpenCL kernel:\n" + std::string(build_log.data()));
        }
        CL_CALL(_kernel = clCreateKernel(_program, "NHWC_TO_NCHW", &_ERR));

        return true;
    }

    std::function<FramePtr()> get_output_allocator() override {
        return [this]() {
            auto output_info = std::make_shared<FrameInfo>(_output_info);
            TensorVector tensors;
            for (auto info : output_info->tensors) {
                cl_mem mem = clCreateBuffer(_opencl_context->context(), 0, info.nbytes(), 0, 0);
                if (!mem)
                    throw std::runtime_error("Error creating OpenCL buffer");
                auto tensor = std::make_shared<OpenCLTensor>(info, _opencl_context, mem);
                tensors.push_back(tensor);
            }
            return std::make_shared<BaseFrame>(MediaType::Tensors, 0, tensors);
        };
    }

    bool process(TensorPtr src, TensorPtr dst) override {
        DLS_CHECK(init());
        auto src_tensor = src.map<OpenCLTensor>(_opencl_context, AccessMode::Read);
        auto dst_tensor = dst.map<OpenCLTensor>(_opencl_context, AccessMode::Write);
        auto &src_info = src_tensor->info();
        auto &dst_info = dst_tensor->info();
        cl_mem src_mem = *src_tensor;
        cl_mem dst_mem = *dst_tensor;
        std::vector<int> src_stride(src_info.stride.begin(), src_info.stride.end());
        std::vector<int> dst_stride(dst_info.stride.begin(), dst_info.stride.end());

        if (ImageLayout(src_info.shape) != ImageLayout::NHWC || src_info.shape[3] != 4)
            throw std::runtime_error("Expect input tensor to have NHWC layout with C=4 (ex, RGBx data)");
        if (ImageLayout(dst_info.shape) != ImageLayout::NCHW || dst_info.shape[1] != 3)
            throw std::runtime_error("Expect output tensor to have NCHW layout with C=3 (ex, RGBP data)");

        // Set arguments
        CL_CALL(_ERR = clSetKernelArg(_kernel, 0, sizeof(cl_mem), &src_mem));
        CL_CALL(_ERR = clSetKernelArg(_kernel, 1, sizeof(cl_mem), &dst_mem));
        CL_CALL(_ERR = clSetKernelArg(_kernel, 2, sizeof(int), &src_stride[0]));
        CL_CALL(_ERR = clSetKernelArg(_kernel, 3, sizeof(int), &src_stride[1]));
        CL_CALL(_ERR = clSetKernelArg(_kernel, 4, sizeof(int), &src_stride[2]));
        CL_CALL(_ERR = clSetKernelArg(_kernel, 5, sizeof(int), &src_stride[3]));
        CL_CALL(_ERR = clSetKernelArg(_kernel, 6, sizeof(int), &dst_stride[0]));
        CL_CALL(_ERR = clSetKernelArg(_kernel, 7, sizeof(int), &dst_stride[1]));
        CL_CALL(_ERR = clSetKernelArg(_kernel, 8, sizeof(int), &dst_stride[2]));
        CL_CALL(_ERR = clSetKernelArg(_kernel, 9, sizeof(int), &dst_stride[3]));

        // Enqueue and flush
        CL_CALL(_ERR = clEnqueueNDRangeKernel(_queue, _kernel, 3, NULL, src_info.shape.data(), nullptr, 0, nullptr,
                                              nullptr));
        CL_CALL(_ERR = clFlush(_queue));

        return true;
    }

    ~OpenclTensorNormalize() {
        if (_queue)
            clReleaseCommandQueue(_queue);
        if (_program)
            clReleaseProgram(_program);
        if (_kernel)
            clReleaseKernel(_kernel);
    }

  private:
    OpenCLContextPtr _opencl_context;
    cl_command_queue _queue = nullptr;
    cl_program _program = nullptr;
    cl_kernel _kernel = nullptr;

    template <typename T>
    void vector_move_element(std::vector<T> &v, size_t old_index, size_t new_index) {
        if (old_index > new_index)
            std::rotate(v.rend() - old_index - 1, v.rend() - old_index, v.rend() - new_index);
        else
            std::rotate(v.begin() + old_index, v.begin() + old_index + 1, v.begin() + new_index + 1);
    }

    static void string_find_and_replace(std::string &data, const std::string &str_search,
                                        const std::string &str_replace) {
        size_t pos = data.find(str_search);
        while (pos != std::string::npos) {
            data.replace(pos, str_search.size(), str_replace);
            pos = data.find(str_search);
        }
    }
};

extern "C" {

DLS_EXPORT ElementDesc opencl_tensor_normalize = {.name = "opencl_tensor_normalize",
                                                  .description =
                                                      "Convert U8 tensor to U8 or F32 tensor with normalization",
                                                  .author = "Intel Corporation",
                                                  .params = nullptr,
                                                  .input_info = {{MediaType::Tensors, MemoryType::OpenCL}},
                                                  .output_info = {{MediaType::Tensors, MemoryType::OpenCL}},
                                                  .create = create_element<OpenclTensorNormalize>,
                                                  .flags = ELEMENT_FLAG_SHARABLE};
}

} // namespace dlstreamer
