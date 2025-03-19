/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/frame.h"
#include "dlstreamer/openvino/tensor.h"

namespace dlstreamer {

class OpenVINOFrame : public BaseFrame {
  public:
    OpenVINOFrame(ov::InferRequest infer_request, ContextPtr context)
        : BaseFrame(MediaType::Tensors, 0, MemoryType::OpenVINO), _infer_request(infer_request) {
        auto num_tensors = _infer_request.get_compiled_model().outputs().size();
        std::function<void()> wait_function = [this] { wait(); };
        for (size_t i = 0; i < num_tensors; i++) {
            auto tensor = std::make_shared<OpenVINOTensor>(infer_request.get_output_tensor(i), context, wait_function);
            _tensors.push_back(tensor);
        }
    }

    operator ov::InferRequest() {
        return _infer_request;
    }

    void set_input(TensorVector tensors) {
        for (size_t i = 0; i < tensors.size(); i++) {
            auto ov_tensors = std::dynamic_pointer_cast<OpenVINOTensorBatch>(tensors[i]);
            if (ov_tensors) {
                _infer_request.set_input_tensors(i, ov_tensors->tensors());
            } else {
                auto ov_tensor = ptr_cast<OpenVINOTensor>(tensors[i]);
                _infer_request.set_input_tensor(i, *ov_tensor);
            }
        }
    }

    void start() {
        _infer_request.start_async();
    }

    void wait() {
        if (_infer_request) {
            _infer_request.wait();
            // After inference is completed the input frame can be released
            set_parent(nullptr);
        }
    }

  protected:
    ov::InferRequest _infer_request;
};

using OpenVINOFramePtr = std::shared_ptr<OpenVINOFrame>;

} // namespace dlstreamer
