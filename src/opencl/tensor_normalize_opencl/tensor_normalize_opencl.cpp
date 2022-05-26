/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tensor_normalize_opencl.h"
#include "dlstreamer/buffer_mappers/opencl_to_cpu.h"
#include <va/va_backend.h>

namespace dlstreamer {

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

static const char *KERNEL_NHWC_TO_NCHW = " \
__kernel void NHWC_TO_NCHW(__global char* src, __global char* dst, \
            const int src_stride0, const int src_stride1, const int src_stride2, const int src_stride3, \
            const int dst_stride0, const int dst_stride1, const int dst_stride2, const int dst_stride3) \
{ \
    /* Indexes for first three dimensions 'NHW', assuming dimension 'C' is equal 4 for src and 3 for dst */ \
    int i0 = get_global_id(0); \
    int i1 = get_global_id(1); \
    int i2 = get_global_id(2); \
\
    int src_idx = i0 * src_stride0 + i1 * src_stride1 + i2 * src_stride2; \
    int dst_idx = i0 * dst_stride0 + i1 * dst_stride2 + i2 * dst_stride3; \
\
    dst[dst_idx] = src[src_idx]; \
    dst[dst_idx + dst_stride1] = src[src_idx + src_stride3]; \
    dst[dst_idx + 2*dst_stride1] = src[src_idx + 2*src_stride3]; \
} \
\
__kernel void IMAGE_TO_NCHW(read_only image2d_t src, __global unsigned char* dst, \
            const int src_height, \
            const int dst_stride0, const int dst_stride1, const int dst_stride2, const int dst_stride3) \
{ \
    /* Indexes for first three dimensions 'NHW', assuming dimension 'C' is equal 4 for src and 3 for dst */ \
    int i0 = get_global_id(0); \
    int i1 = get_global_id(1); \
    int i2 = get_global_id(2); \
\
    uint4 val = read_imageui(src, (int2)(i2, i0*src_height + i1));\
    int dst_idx = i0 * dst_stride0 + i1 * dst_stride2 + i2 * dst_stride3; \
\
    dst[dst_idx] = val.x; \
    dst[dst_idx + dst_stride1] = val.y; \
    dst[dst_idx + 2*dst_stride1] = val.z; \
} \
";

class TensorNormalizeOpenCL : public TransformWithAlloc {
  public:
    using TransformWithAlloc::TransformWithAlloc;

    BufferInfoVector get_input_info(const BufferInfo &output_info) override {
        if (output_info.planes.empty()) {
            return TensorNormalizeOpenCLDesc.input_info;
        } else {
            BufferInfo input_info = output_info;
            // Move C dimension to last dimension
            for (size_t i = 0; i < input_info.planes.size(); i++) {
                PlaneInfo &plane = input_info.planes[i];
                std::vector<size_t> shape = plane.shape;
                size_t c_position = plane.layout.c_position();
                if (shape[c_position] == 3) // RGB to RGBx
                    shape[c_position] = 4;
                vector_move_element(shape, c_position, shape.size() - 1);
                plane = PlaneInfo(shape, plane.type, plane.name);
            }
            return {input_info};
        }
    }
    BufferInfoVector get_output_info(const BufferInfo &input_info) override {
        if (input_info.planes.empty()) {
            return TensorNormalizeOpenCLDesc.output_info;
        } else {
            BufferInfo output_info = input_info;
            // Move C dimension before HW
            for (size_t i = 0; i < output_info.planes.size(); i++) {
                PlaneInfo &plane = output_info.planes[i];
                std::vector<size_t> shape = plane.shape;
                size_t c_position = plane.layout.c_position();
                if (shape[c_position] == 4) // RGBx to RGB
                    shape[c_position] = 3;
                vector_move_element(shape, c_position, shape.size() - 3);
                plane = PlaneInfo(shape, plane.type, plane.name);
            }
            return {output_info};
        }
    }
    void set_info(const BufferInfo &input_info, const BufferInfo &output_info) override {
        _input_info = input_info;
        _output_info = output_info;

        _opencl_context = _transform_ctrl->get_context<OpenCLContext>();
        if (!_opencl_context)
            throw std::runtime_error("Can't query OpenCL context ");
        auto cl_ctx = _opencl_context->context();

        _in_mapper = _transform_ctrl->create_input_mapper(BufferType::OPENCL_BUFFER, _opencl_context);

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
        CL_CALL(_program = clCreateProgramWithSource(cl_ctx, 1, &KERNEL_NHWC_TO_NCHW, NULL, &_ERR));
        CL_CALL(_ERR = clBuildProgram(_program, 1, devices.data(), NULL, NULL, NULL));
        CL_CALL(_kernel_for_buffers = clCreateKernel(_program, "NHWC_TO_NCHW", &_ERR));
        CL_CALL(_kernel_for_images = clCreateKernel(_program, "IMAGE_TO_NCHW", &_ERR));
    }

    ContextPtr get_context(const std::string & /*name*/) override {
        return nullptr;
    }
    std::function<BufferPtr()> get_output_allocator() override {
        return [this]() {
            auto output_info = std::make_shared<BufferInfo>(_output_info);
            std::vector<cl_mem> mem(output_info->planes.size());
            for (size_t i = 0; i < mem.size(); i++) {
                mem[i] = clCreateBuffer(_opencl_context->context(), 0, output_info->planes[i].size(), 0, 0);
                if (!mem[i])
                    throw std::runtime_error("Error creating OpenCL buffer");
            }
            return std::make_shared<OpenCLBuffer>(output_info, _opencl_context, mem);
        };
    }
    BufferMapperPtr get_output_mapper() override {
        return std::make_shared<BufferMapperOpenCLToCPU>();
    }
    bool process(BufferPtr src, BufferPtr dst) override {
        auto src_opencl = _in_mapper->map<OpenCLBuffer>(src, AccessMode::READ);
        if (!src_opencl)
            throw std::runtime_error("Error mapping to OpenCLBuffer");
        auto dst_opencl = std::dynamic_pointer_cast<OpenCLBuffer>(dst);
        if (!dst_opencl)
            throw std::runtime_error("Failed to dynamically cast Buffer to OpenCLBuffer");
        cl_mem src_mem = src_opencl->clmem(0);
        cl_mem dst_mem = dst_opencl->clmem(0);
        const PlaneInfo &src_info = src->info()->planes[0];
        const PlaneInfo &dst_info = dst->info()->planes[0];
        std::vector<int> src_stride(src_info.stride.begin(), src_info.stride.end());
        std::vector<int> dst_stride(dst_info.stride.begin(), dst_info.stride.end());

        if (src_info.layout != Layout::NHWC || src_info.shape[3] != 4)
            throw std::runtime_error("Expect input tensor to have NHWC layout with C=4 (ex, RGBx data)");
        if (dst_info.layout != Layout::NCHW || dst_info.shape[1] != 3)
            throw std::runtime_error("Expect output tensor to have NCHW layout with C=3 (ex, RGBP data)");

        cl_kernel _kernel;
        cl_mem_object_type obj_type = 0;
        clGetMemObjectInfo(src_mem, CL_MEM_TYPE, sizeof(obj_type), &obj_type, 0);
        if (obj_type == CL_MEM_OBJECT_BUFFER) {
            _kernel = _kernel_for_buffers;
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
        } else if (obj_type == CL_MEM_OBJECT_IMAGE2D) {
            _kernel = _kernel_for_images;
            int src_height = src_info.height();
            CL_CALL(_ERR = clSetKernelArg(_kernel, 0, sizeof(cl_mem), &src_mem));
            CL_CALL(_ERR = clSetKernelArg(_kernel, 1, sizeof(cl_mem), &dst_mem));
            CL_CALL(_ERR = clSetKernelArg(_kernel, 2, sizeof(int), &src_height));
            CL_CALL(_ERR = clSetKernelArg(_kernel, 3, sizeof(int), &dst_stride[0]));
            CL_CALL(_ERR = clSetKernelArg(_kernel, 4, sizeof(int), &dst_stride[1]));
            CL_CALL(_ERR = clSetKernelArg(_kernel, 5, sizeof(int), &dst_stride[2]));
            CL_CALL(_ERR = clSetKernelArg(_kernel, 6, sizeof(int), &dst_stride[3]));
        } else {
            throw std::runtime_error("Unsupported OpenCL memory object type");
        }

        CL_CALL(_ERR = clEnqueueNDRangeKernel(_queue, _kernel, 3, NULL, src_info.shape.data(), nullptr, 0, nullptr,
                                              nullptr));
        CL_CALL(_ERR = clFlush(_queue));

        return true;
    }
    ~TensorNormalizeOpenCL() {
        if (_queue)
            clReleaseCommandQueue(_queue);
        if (_program)
            clReleaseProgram(_program);
        if (_kernel_for_buffers)
            clReleaseKernel(_kernel_for_buffers);
        if (_kernel_for_images)
            clReleaseKernel(_kernel_for_images);
    }

  private:
    BufferInfo _input_info;
    BufferInfo _output_info;
    BufferMapperPtr _in_mapper;
    OpenCLContextPtr _opencl_context;
    cl_command_queue _queue = nullptr;
    cl_program _program = nullptr;
    cl_kernel _kernel_for_buffers = nullptr;
    cl_kernel _kernel_for_images = nullptr;

    template <typename T>
    void vector_move_element(std::vector<T> &v, size_t old_index, size_t new_index) {
        if (old_index > new_index)
            std::rotate(v.rend() - old_index - 1, v.rend() - old_index, v.rend() - new_index);
        else
            std::rotate(v.begin() + old_index, v.begin() + old_index + 1, v.begin() + new_index + 1);
    }
};

TransformDesc TensorNormalizeOpenCLDesc = {.name = "tensor_normalize_opencl",
                                           .description = "Convert U8 tensor to U8 or F32 tensor with normalization",
                                           .author = "Intel Corporation",
                                           .params = nullptr,
                                           .input_info = {{MediaType::TENSORS, BufferType::OPENCL_BUFFER}},
                                           .output_info = {{MediaType::TENSORS, BufferType::OPENCL_BUFFER}},
                                           .create = TransformBase::create<TensorNormalizeOpenCL>,
                                           .flags = TRANSFORM_FLAG_OUTPUT_ALLOCATOR | TRANSFORM_FLAG_SHARABLE};

} // namespace dlstreamer
