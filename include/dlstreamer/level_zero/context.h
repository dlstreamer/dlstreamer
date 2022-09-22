/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <dlstreamer/base/context.h>
#include <dlstreamer/level_zero/mappers/dma_to_usm.h>
#include <dlstreamer/level_zero/mappers/usm_to_dma.h>

#include <level_zero/ze_api.h>

namespace dlstreamer {

class LevelZeroContext : public BaseContext {
  public:
    struct key {
        static constexpr auto ze_device = BaseContext::key::ze_device;
        static constexpr auto ze_context = BaseContext::key::ze_context;
    };

    LevelZeroContext(ze_context_handle_t ze_context, ze_device_handle_t ze_device = nullptr)
        : BaseContext(MemoryType::USM), _ze_context(ze_context), _ze_device(ze_device) {
    }

    ze_context_handle_t ze_context() const noexcept {
        return _ze_context;
    }

    ze_device_handle_t ze_device() const noexcept {
        return _ze_device;
    }

    handle_t handle(std::string_view key) const noexcept override {
        if (key == key::ze_context)
            return _ze_context;
        if (key == key::ze_device)
            return _ze_device;
        return nullptr;
    }

    std::vector<std::string> keys() const override {
        return {key::ze_context, key::ze_device};
    }

    MemoryMapperPtr get_mapper(const ContextPtr &input_context, const ContextPtr &output_context) override {
        auto mapper = BaseContext::get_mapper(input_context, output_context);
        if (mapper)
            return mapper;

        auto input_type = input_context ? input_context->memory_type() : MemoryType::CPU;
        auto output_type = output_context ? output_context->memory_type() : MemoryType::CPU;
        if (input_type == MemoryType::USM && output_type == MemoryType::DMA)
            mapper = std::make_shared<MemoryMapperUSMToDMA>(input_context, output_context);
        if (input_type == MemoryType::DMA && output_type == MemoryType::USM)
            mapper = std::make_shared<MemoryMapperDMAToUSM>(input_context, output_context);

        BaseContext::attach_mapper(mapper);
        return mapper;
    }

  private:
    ze_context_handle_t _ze_context;
    ze_device_handle_t _ze_device;
};

} // namespace dlstreamer
