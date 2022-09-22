/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/opencv/elements/tensor_normalize_opencv.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/cpu/frame_alloc.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/opencv/tensor.h"

namespace dlstreamer {

namespace param {
static constexpr auto range = "range";
static constexpr auto mean = "mean";
static constexpr auto std = "std";
}; // namespace param

static ParamDescVector params_desc = {
    {param::range, "Normalization range MIN, MAX. Example: <0,1>", std::vector<double>()},
    {param::mean, "Mean values per channel. Example: <0.485,0.456,0.406>", std::vector<double>()},
    {param::std, "Standard deviation values per channel. Example: <0.229,0.224,0.225>", std::vector<double>()},
};

class TensorNormalizeOpenCV : public BaseTransform {
  public:
    TensorNormalizeOpenCV(DictionaryCPtr params, const ContextPtr &app_context) : BaseTransform(app_context) {
        _range = params->get<std::vector<double>>(param::range, {});
        _mean = params->get<std::vector<double>>(param::mean, {});
        _std = params->get<std::vector<double>>(param::std, {});
    }

    FrameInfoVector get_input_info() override {
        if (_output_info.tensors.empty()) {
            return tensor_normalize_opencv.input_info;
        } else {
            FrameInfo info = _output_info;
            info.tensors[0].dtype = DataType::UInt8;
            return {info};
        }
    }

    FrameInfoVector get_output_info() override {
        if (_input_info.tensors.empty()) {
            return tensor_normalize_opencv.output_info;
        } else {
            FrameInfo info = _input_info;
            info.tensors[0].dtype = DataType::Float32;
            return {info};
        }
    }

    std::function<FramePtr()> get_output_allocator() override {
        return [this]() { return std::make_shared<CPUFrameAlloc>(_output_info); };
    }

    bool process(TensorPtr src, TensorPtr dst) override {
        auto src_tensor = src.map(AccessMode::Read);
        auto dst_tensor = dst.map(AccessMode::Write);
        ImageInfo src_info(src_tensor->info());
        ImageInfo dst_info(dst_tensor->info());
        int w = src_info.width();
        int h = src_info.height();
        int channels = src_info.channels();
        int batch = src_info.batch();

        uint8_t *src_data = src_tensor->data<uint8_t>();
        uint8_t *dst_data = dst_tensor->data<uint8_t>();
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
    std::vector<double> _range;
    std::vector<double> _mean;
    std::vector<double> _std;
};

extern "C" {
ElementDesc tensor_normalize_opencv = {
    .name = "tensor_normalize_opencv",
    .description = "Convert U8 tensor to F32 tensor with normalization",
    .author = "Intel Corporation",
    .params = &params_desc,
    .input_info = {FrameInfo(MediaType::Tensors, MemoryType::CPU, {{{}, DataType::UInt8}})},
    .output_info = {FrameInfo(MediaType::Tensors, MemoryType::CPU, {{{}, DataType::Float32}})},
    .create = create_element<TensorNormalizeOpenCV>,
    .flags = 0};
}

} // namespace dlstreamer
