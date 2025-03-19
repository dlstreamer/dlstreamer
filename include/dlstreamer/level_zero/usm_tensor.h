/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <dlstreamer/base/frame.h>

#include <level_zero/ze_api.h>

namespace dlstreamer {

class USMTensor : public BaseTensor {
  public:
    USMTensor(const TensorInfo &info, void *data, bool take_ownership, ContextPtr context)
        : BaseTensor(MemoryType::USM, info, "", context), _data(data), _take_ownership(take_ownership) {
        if (take_ownership && !context)
            throw std::runtime_error("No context in USMTensor");
    }

    ~USMTensor() {
        if (_take_ownership && _data) {
            ze_context_handle_t ze_context = ze_context_handle_t(_context->handle(BaseContext::key::ze_context));
            zeMemFree(ze_context, _data);
        }
    }

    void *data() const override {
        return _data;
    }

  private:
    void *_data;
    bool _take_ownership;
};

using USMTensorPtr = std::shared_ptr<USMTensor>;

} // namespace dlstreamer
