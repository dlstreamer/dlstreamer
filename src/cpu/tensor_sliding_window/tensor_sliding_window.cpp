/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/cpu/elements/tensor_sliding_window.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/cpu/frame_alloc.h"
#include "dlstreamer/memory_mapper_factory.h"

#include <deque>

namespace dlstreamer {

class TensorSlidingWindow : public BaseTransform {
  public:
    TensorSlidingWindow(DictionaryCPtr /*params*/, const ContextPtr &app_context) : BaseTransform(app_context) {
    }

    std::function<FramePtr()> get_output_allocator() override {
        DLS_CHECK(_input_info.tensors.size() && _input_info.tensors[0].size())
        DLS_CHECK(_output_info.tensors.size() && _output_info.tensors[0].size())
        _aggregate_size = _output_info.tensors[0].size() / _input_info.tensors[0].size();

        return [this]() { return std::make_shared<CPUFrameAlloc>(_output_info); };
    }

    bool process(TensorPtr src, TensorPtr dst) override {
        auto src_tensor = src.map(AccessMode::Read);
        float *src_data = src_tensor->data<float>();
        const auto data_size = src_tensor->info().size();

        _aggregate_tensors.push_back(std::vector<float>(src_data, src_data + data_size));

        // TODO: uncomment below condition once the bug that it is freezing when return false
        // if (_aggregate_tensors.size() < _aggregate_size)
        //    return false;

        auto dst_tensor = dst.map(AccessMode::Write);
        float *dst_data = dst_tensor->data<float>();
        for (auto tensor : _aggregate_tensors) {
            std::copy(tensor.begin(), tensor.end(), dst_data);
            dst_data += tensor.size();
        }

        while (_aggregate_tensors.size() >= _aggregate_size)
            _aggregate_tensors.pop_front();

        return true;
    }

  private:
    std::deque<std::vector<float>> _aggregate_tensors;
    size_t _aggregate_size = 0;
};

extern "C" {
ElementDesc tensor_sliding_window = {.name = "tensor_sliding_window",
                                     .description = "Sliding aggregation of input tensors",
                                     .author = "Intel Corporation",
                                     .params = nullptr,
                                     .input_info = MAKE_FRAME_INFO_VECTOR({{MediaType::Tensors, MemoryType::Any}}),
                                     .output_info = MAKE_FRAME_INFO_VECTOR({{MediaType::Tensors, MemoryType::CPU}}),
                                     .create = create_element<TensorSlidingWindow>,
                                     .flags = 0};
}

} // namespace dlstreamer
