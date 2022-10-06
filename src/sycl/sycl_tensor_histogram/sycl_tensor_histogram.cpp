/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <CL/sycl.hpp>

#include "base_histogram.h"
#include "dlstreamer/cpu/frame_alloc.h"
#include "dlstreamer/cpu/utils.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/sycl/context.h"
#include "dlstreamer/sycl/elements/sycl_tensor_histogram.h"
#include "dlstreamer/sycl/sycl_usm_tensor.h"
#include "dlstreamer/utils.h"
#include "dlstreamer/vaapi/context.h"
#include "dlstreamer/vaapi/frame_alloc.h"
#include <cmath>
#include <cstring>

namespace dlstreamer {

class SyclTensorHistogram : public BaseHistogram {
  public:
    SyclTensorHistogram(DictionaryCPtr params, const ContextPtr &app_context) : BaseHistogram(params, app_context) {
        _queue = sycl::queue(sycl::gpu_selector()); // TODO handle device parameter
    }

    ~SyclTensorHistogram() {
        if (_weight)
            sycl::free(_weight, _queue);
        if (_src_data)
            sycl::free(_src_data, _queue);
        if (_dst_data)
            sycl::free(_dst_data, _queue);
    }

    FrameInfoVector get_input_info() override {
        return {{MediaType::Tensors, MemoryType::USM, {{{_batch_size, _height, _width, 3}}}},
                {MediaType::Tensors, MemoryType::USM, {{{_batch_size, _height, _width, 4}}}},
                {MediaType::Tensors, MemoryType::VAAPI, {{{_batch_size, _height, _width, 3}}}},
                {MediaType::Tensors, MemoryType::VAAPI, {{{_batch_size, _height, _width, 4}}}}};
    }

    bool init_once() override {
        _sycl_context = SYCLContext::create(_queue);

        if (_input_info.memory_type == MemoryType::VAAPI) {
            _vaapi_context = VAAPIContext::create(_app_context);
            _dma_context = DMAContext::create(_app_context);
            create_mapper({_app_context, _vaapi_context, _dma_context, _sycl_context}, true);
        }

        _weight = _sycl_context->malloc<float>(_slice_h * _slice_w, sycl::usm::alloc::shared);
        fill_weights(_weight);

        // src and dst pointers per slice
        ImageInfo src_info(_input_info.tensors[0]);
        size_t num_slices = src_info.batch() * _num_slices_y * _num_slices_x;
        _src_data = _sycl_context->malloc<uint8_t *>(num_slices, sycl::usm::alloc::shared);
        _dst_data = _sycl_context->malloc<float *>(num_slices, sycl::usm::alloc::shared);

        return true;
    }

    std::function<FramePtr()> get_output_allocator() override {
        return [this]() {
            TensorVector tensors = {
                std::make_shared<SYCLUSMTensor>(_output_info.tensors[0], _sycl_context, sycl::usm::alloc::shared)};
            return std::make_shared<BaseFrame>(MediaType::Tensors, 0, tensors);
        };
    }

    bool process(TensorPtr src, TensorPtr dst) override {
        auto src_tensor = src.map(_sycl_context, AccessMode::Read);
        auto dst_tensor = dst; // dst.map(_sycl_context, AccessMode::Write);
        ImageInfo src_info(src_tensor->info());
        DLS_CHECK(src_info.layout() == ImageLayout::NHWC);
        DLS_CHECK(src_info.width() == _width && src_info.height() == _height);

        // reshape dst_tensor to have _num_slices_y and _num_slices_x as dimensions
        std::vector<size_t> dst_shape = {_batch_size, _num_slices_y, _num_slices_x, _num_bins * _num_bins * _num_bins};
        TensorInfo dst_info(dst_shape, DataType::Float32);
        DLS_CHECK(dst_info.nbytes() == dst_tensor->info().nbytes());
        auto dst_reshaped = std::make_shared<CPUTensor>(dst_info, dst_tensor->data());

        sycl::event event = _queue.memset(dst->data(), 0, dst_info.nbytes());
        event.wait();

        // src and dst pointers per slice
        size_t i = 0;
        for (size_t b = 0; b < src_info.batch(); b++) {
            for (size_t y = 0; y < _num_slices_y; y++) {
                for (size_t x = 0; x < _num_slices_x; x++) {
                    auto src_slice =
                        get_tensor_slice(src_tensor, {{b, 1}, {y * _slice_h, _slice_h}, {x * _slice_w, _slice_w}});
                    auto dst_slice = get_tensor_slice(dst_reshaped, {{b, 1}, {y, 1}, {x, 1}});

                    _src_data[i] = src_slice->data<uint8_t>();
                    _dst_data[i] = dst_slice->data<float>();
                    i++;
                }
            }
        }

        // local copy of parameters required for SYCL kernel
        size_t stride = src_info.width_stride();
        size_t num_channels = src_info.channels();
        size_t slice_w = _slice_w;
        size_t slice_h = _slice_h;
        size_t bin_size = _bin_size;
        size_t num_bins = _num_bins;
        float *weight = _weight;
        uint8_t **src_data = _src_data;
        float **dst_data = _dst_data;

        // submit kernel
        size_t num_slices = src_info.batch() * _num_slices_y * _num_slices_x;
        _queue
            .parallel_for(num_slices,
                          [=](auto i) {
                              uint8_t *src_ptr = src_data[i];
                              float *dst_ptr = dst_data[i];
                              for (size_t y = 0; y < slice_h; y++) {
                                  for (size_t x = 0; x < slice_w; x++) {
                                      auto rgb_data = src_ptr + x * num_channels;
                                      int32_t index0 = rgb_data[0] / bin_size;
                                      int32_t index1 = rgb_data[1] / bin_size;
                                      int32_t index2 = rgb_data[2] / bin_size;
                                      int32_t hist_index = num_bins * (num_bins * index0 + index1) + index2;
                                      dst_ptr[hist_index] += weight[y * slice_w + x];
                                  }
                                  src_ptr += stride;
                              }
                          })
            .wait();

        return true;
    }

  private:
    sycl::queue _queue;
    ContextPtr _vaapi_context;
    ContextPtr _dma_context;
    SYCLContextPtr _sycl_context;

    float *_weight = nullptr;
    uint8_t **_src_data = nullptr;
    float **_dst_data = nullptr;
};

extern "C" {
ElementDesc sycl_tensor_histogram = {
    .name = "sycl_tensor_histogram",
    .description = "Calculates histogram on tensors of UInt8 data type and NHWC layout",
    .author = "Intel Corporation",
    .params = &SyclTensorHistogram::params_desc,
    .input_info = {FrameInfo(MediaType::Tensors, MemoryType::USM, {{{}, DataType::UInt8}}),
                   FrameInfo(MediaType::Tensors, MemoryType::VAAPI, {{{}, DataType::UInt8}})},
    .output_info = {FrameInfo(MediaType::Tensors, MemoryType::CPU, {{{}, DataType::Float32}})},
    .create = create_element<SyclTensorHistogram>,
    .flags = 0};
}

} // namespace dlstreamer
