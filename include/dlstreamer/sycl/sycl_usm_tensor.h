/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <CL/sycl.hpp>
#include <dlstreamer/base/frame.h>
#include <dlstreamer/sycl/context.h>

namespace dlstreamer {

class SYCLUSMTensor : public BaseTensor {
  public:
    struct key {
        static constexpr auto data = "data";         // void*
        static constexpr auto usm_type = "usm_type"; // enum sycl::usm::alloc
    };

    SYCLUSMTensor(const TensorInfo &info, ContextPtr context, sycl::usm::alloc usm_type)
        : BaseTensor(MemoryType::USM, info, key::data, context), _usm_type(usm_type), _take_ownership(true) {
        _data = ptr_cast<SYCLContext>(context)->malloc<uint8_t>(info.nbytes(), usm_type);
        set_handle(key::data, reinterpret_cast<handle_t>(_data));
        set_handle(key::usm_type, static_cast<handle_t>(usm_type));
    }

    ~SYCLUSMTensor() {
        if (_take_ownership && _data) {
            ptr_cast<SYCLContext>(_context)->free(_data);
        }
    }

    void *data() const override {
        return _data;
    }

    sycl::usm::alloc usm_type() {
        return _usm_type;
    }

  private:
    void *_data;
    sycl::usm::alloc _usm_type;
    bool _take_ownership;
};

using SYCLUSMTensorPtr = std::shared_ptr<SYCLUSMTensor>;

} // namespace dlstreamer
