/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tensor_normalize_opencv.h"
#include "dlstreamer/buffer_mappers/mapper_chain.h"
#include "dlstreamer/opencv/buffer.h"
#include "dlstreamer/utils.h"

namespace dlstreamer {

namespace param {
static constexpr auto range = "range";
static constexpr auto mean = "mean";
static constexpr auto std = "std";
}; // namespace param

static ParamDescVector params_desc = {
    {param::range, "Normalization range in MIN,MAX string format, example: 0.0,1.0", ""},
    {param::mean, "Comma-separated mean values per channel, example: 0.485,0.456,0.406", ""},
    {param::std, "Comma-separated standard deviation values per channel, example: 0.229,0.224,0.225", ""},
};

class TensorNormalizeOpenCV : public TransformWithAlloc {
  public:
    TensorNormalizeOpenCV(ITransformController &transform_ctrl, DictionaryCPtr params)
        : TransformWithAlloc(transform_ctrl, std::move(params)) {
        _range = string_to_float_array(_params->get<std::string>(param::range, ""));
        _mean = string_to_float_array(_params->get<std::string>(param::mean, ""));
        _std = string_to_float_array(_params->get<std::string>(param::std, ""));
    }

    BufferInfoVector get_input_info(const BufferInfo &output_info) override {
        if (output_info.planes.empty()) {
            return TensorNormalizeOpenCVDesc.input_info;
        } else {
            BufferInfo info = output_info;
            info.planes[0].type = DataType::U8;
            return {info};
        }
    }

    BufferInfoVector get_output_info(const BufferInfo &input_info) override {
        if (input_info.planes.empty()) {
            return TensorNormalizeOpenCVDesc.output_info;
        } else {
            BufferInfo info = input_info;
            info.planes[0].type = DataType::FP32;
            return {info};
        }
    }

    void set_info(const BufferInfo &input_info, const BufferInfo &output_info) override {
        _input_info = input_info;
        _output_info = output_info;
        _cpu_mapper = _transform_ctrl->create_input_mapper(BufferType::CPU);
    }

    ContextPtr get_context(const std::string & /*name*/) override {
        return nullptr;
    }

    std::function<BufferPtr()> get_output_allocator() override {
        return [this]() {
            auto output_info = std::make_shared<BufferInfo>(_output_info);
            std::vector<void *> data;
            for (auto &plane : output_info->planes) {
                data.push_back(malloc(plane.size()));
            }
            auto deleter = [data](CPUBuffer *dst) {
                for (auto ptr : data)
                    free(ptr);
                delete dst;
            };
            return CPUBufferPtr(new CPUBuffer(output_info, data), deleter);
        };
    }

    BufferMapperPtr get_output_mapper() override {
        return nullptr;
    }

    bool process(BufferPtr src, BufferPtr dst) override {
        if (src->info()->planes.size() != 1 || dst->info()->planes.size() != 1)
            throw std::runtime_error("Expect single plane buffers");
        auto &src_info = src->info()->planes[0];
        auto &dst_info = dst->info()->planes[0];
        int w = src_info.width();
        int h = src_info.height();
        int channels = src_info.channels();
        int batch = (src_info.shape.size() >= 4) ? src_info.batch() : 1;

        auto src_cpu = _cpu_mapper->map<CPUBuffer>(src, AccessMode::READ);
        char *src_data = static_cast<char *>(src_cpu->data());
        char *dst_data = static_cast<char *>(dst->data());
        size_t src_w_stride = src_info.width_stride();
        size_t dst_w_stride = dst_info.width_stride();
        size_t src_h_stride = src_info.height_stride();
        size_t dst_h_stride = dst_info.height_stride();
        size_t src_c_stride = (batch > 1) ? src_info.channels_stride() : 0;
        size_t dst_c_stride = (batch > 1) ? dst_info.channels_stride() : 0;

        for (int j = 0; j < batch; j++) {
            for (int i = 0; i < channels; i++) {
                double alpha = 1;
                double beta = 0;
                if (!_range.empty()) {
                    alpha = (_range[1] - _range[0]) / 255.f;
                    beta = _range[0];
                }
                if (!_std.empty()) {
                    alpha = _std[i];
                }
                if (!_mean.empty()) {
                    beta = _mean[i];
                }
                cv::Mat src_mat = cv::Mat(h, w, CV_8UC1, src_data + i * src_h_stride, src_w_stride);
                cv::Mat dst_mat = cv::Mat(h, w, CV_32FC1, dst_data + i * dst_h_stride, dst_w_stride);
                src_mat.convertTo(dst_mat, dst_mat.type(), alpha, beta);
            }
            src_data += src_c_stride;
            dst_data += dst_c_stride;
        }
        return true;
    }

  private:
    BufferInfo _input_info;
    BufferInfo _output_info;
    BufferMapperPtr _cpu_mapper;

    std::vector<float> _range;
    std::vector<float> _mean;
    std::vector<float> _std;
};

TransformDesc TensorNormalizeOpenCVDesc = {
    .name = "tensor_normalize_opencv",
    .description = "Convert U8 tensor to F32 tensor with normalization",
    .author = "Intel Corporation",
    .params = &params_desc,
    .input_info = {BufferInfo(MediaType::TENSORS, BufferType::CPU, {{{}, DataType::U8}})},
    .output_info = {BufferInfo(MediaType::TENSORS, BufferType::CPU, {{{}, DataType::FP32}})},
    .create = TransformBase::create<TensorNormalizeOpenCV>,
    .flags = TRANSFORM_FLAG_OUTPUT_ALLOCATOR | TRANSFORM_FLAG_SUPPORT_PARAMS_STRUCTURE};

} // namespace dlstreamer
