/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/cpu/elements/tensor_histogram.h"
#include "base_histogram.h"
#include "dlstreamer/cpu/frame_alloc.h"
#include "dlstreamer/cpu/utils.h"
#include "dlstreamer/memory_mapper_factory.h"
#include <cmath>
#include <cstring>

namespace dlstreamer {

class TensorHistogramCPU : public BaseHistogram {
  public:
    using BaseHistogram::BaseHistogram;

    bool init_once() override {
        try {
            _weight = std::make_unique<float[]>(_slice_h * _slice_w);
        } catch (...) {
            return false;
        }
        fill_weights(_weight.get());
        return true;
    }

    std::function<FramePtr()> get_output_allocator() override {
        return [this]() { return std::make_shared<CPUFrameAlloc>(_output_info); };
    }

    bool process(TensorPtr src, TensorPtr dst) override {
        auto src_tensor = src.map(AccessMode::Read);
        auto dst_tensor = dst.map(AccessMode::Write);
        ImageInfo src_info(src_tensor->info());
        DLS_CHECK(src_info.layout() == ImageLayout::NHWC);
        DLS_CHECK(src_info.width() == _width && src_info.height() == _height);

        // reshape dst_tensor to have _num_slices_y and _num_slices_x as dimensions
        std::vector<size_t> dst_shape = {_batch_size, _num_slices_y, _num_slices_x, _num_bins * _num_bins * _num_bins};
        TensorInfo dst_info(dst_shape, DataType::Float32);
        DLS_CHECK(dst_info.nbytes() == dst_tensor->info().nbytes());
        auto dst_reshaped = std::make_shared<CPUTensor>(dst_info, dst_tensor->data());

        for (size_t b = 0; b < src_info.batch(); b++) {
            for (size_t y = 0; y < _num_slices_y; y++) {
                for (size_t x = 0; x < _num_slices_x; x++) {
                    auto src_slice =
                        get_tensor_slice(src_tensor, {{b, 1}, {y * _slice_h, _slice_h}, {x * _slice_w, _slice_w}});
                    auto dst_slice = get_tensor_slice(dst_reshaped, {{b, 1}, {y, 1}, {x, 1}});
                    calc_slice_histogram(src_slice, dst_slice);
                }
            }
        }
        return true;
    }

    void calc_slice_histogram(TensorPtr src, TensorPtr dst) {
        ImageInfo src_info(src->info());
        TensorInfo dst_info = dst->info();
        auto stride = src_info.width_stride();
        uint8_t *src_data = src->data<uint8_t>();
        float *dst_data = dst->data<float>();

        DLS_CHECK(src_info.width() == _slice_w && src_info.height() == _slice_h);
        DLS_CHECK(dst_info.size() == _num_bins * _num_bins * _num_bins);
        size_t num_channels = src_info.channels();
        DLS_CHECK(num_channels == 3 || num_channels == 4);

        memset(dst_data, 0, dst_info.size() * sizeof(float));
        for (size_t y = 0; y < _slice_h; y++) {
            for (size_t x = 0; x < _slice_w; x++) {
                auto rgb_data = src_data + x * num_channels;
                int32_t index0 = rgb_data[0] / _bin_size;
                int32_t index1 = rgb_data[1] / _bin_size;
                int32_t index2 = rgb_data[2] / _bin_size;
                int32_t hist_index = _num_bins * (_num_bins * index0 + index1) + index2;
                dst_data[hist_index] += _weight[y * _slice_w + x];
            }
            src_data += stride;
        }
    }

  private:
    std::unique_ptr<float[]> _weight;
};

extern "C" {
ElementDesc tensor_histogram = {
    .name = "tensor_histogram",
    .description = "Calculates histogram on tensors of UInt8 data type and NHWC layout",
    .author = "Intel Corporation",
    .params = &TensorHistogramCPU::params_desc,
    .input_info = MAKE_FRAME_INFO_VECTOR({FrameInfo(MediaType::Tensors, MemoryType::Any, {{{}, DataType::UInt8}})}),
    .output_info = MAKE_FRAME_INFO_VECTOR({FrameInfo(MediaType::Tensors, MemoryType::CPU, {{{}, DataType::Float32}})}),
    .create = create_element<TensorHistogramCPU>,
    .flags = 0};
}

} // namespace dlstreamer
