/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/base/transform.h"
#include <cmath>
#include <cstring>

namespace dlstreamer {

class BaseHistogram : public BaseTransform {
  public:
    struct param {
        static constexpr auto width = "width";
        static constexpr auto height = "height";
        static constexpr auto num_slices_x = "num-slices-x";
        static constexpr auto num_slices_y = "num-slices-y";
        static constexpr auto num_bins = "num-bins";
        static constexpr auto batch_size = "batch-size";
        static constexpr auto device = "device";
    };

    static ParamDescVector params_desc;

    BaseHistogram(DictionaryCPtr params, const ContextPtr &app_context) : BaseTransform(app_context) {
        _width = params->get<int>(param::width);
        _height = params->get<int>(param::height);
        _num_slices_x = params->get<int>(param::num_slices_x);
        _num_slices_y = params->get<int>(param::num_slices_y);

        // slice size and weight
        _slice_w = _width / _num_slices_x;
        _slice_h = _height / _num_slices_y;

        // histogram bins
        _num_bins = params->get<int>(param::num_bins);
        _bin_size = 256 / _num_bins;

        // batch_size
        _batch_size = params->get<int>(param::batch_size);
    }

    FrameInfoVector get_input_info() override {
        return {{MediaType::Tensors, MemoryType::Any, {{{_batch_size, _height, _width, 3}}}},
                {MediaType::Tensors, MemoryType::Any, {{{_batch_size, _height, _width, 4}}}}};
    }

    FrameInfoVector get_output_info() override {
        std::vector<size_t> shape = {_batch_size, _num_slices_y * _num_slices_x * _num_bins * _num_bins * _num_bins};
        FrameInfo hist_output_info = {MediaType::Tensors, MemoryType::CPU, {{shape, DataType::Float32}}};
        return {hist_output_info};
    }

  protected:
    size_t _width;
    size_t _height;
    size_t _num_slices_x;
    size_t _num_slices_y;
    size_t _slice_w;
    size_t _slice_h;
    size_t _bin_size;
    size_t _num_bins;
    size_t _batch_size;

    void fill_weights(float *weight) {
        const float sigma_x = 0.5f * _slice_w;
        const float sigma_y = 0.5f * _slice_h;
        for (size_t y = 0; y < _slice_h; ++y) {
            float dy = (0.5f * _slice_h - y) / sigma_y;
            for (size_t x = 0; x < _slice_w; ++x) {
                float dx = (0.5f * _slice_w - x) / sigma_x;
                weight[y * _slice_w + x] = expf(-0.5f * (dx * dx + dy * dy));
            }
        }
    }
};

ParamDescVector BaseHistogram::params_desc = {
    {param::width, "Input tensor width, assuming tensor in NHWC or NCHW layout", 64},
    {param::height, "Input tensor height, assuming tensor in NHWC or NCHW layout", 64},
    {param::num_slices_x, "Number slices along X-axis", 1},
    {param::num_slices_y, "Number slices along Y-axis", 1},
    {param::num_bins,
     "Number bins in histogram calculation. Example, for 3-channel tensor (RGB image), "
     "output histogram size is equal to (num_bin^3 * num_slices_x * num_slices_y)",
     8},
    {param::batch_size, "Batch size", 1, 0, std::numeric_limits<int>::max()},
    {param::device, "CPU or GPU or GPU.0, GPU.1, ..", ""},
};

} // namespace dlstreamer
