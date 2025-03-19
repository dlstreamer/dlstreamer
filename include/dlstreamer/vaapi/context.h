/*******************************************************************************
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/context.h"
#include "dlstreamer/utils.h"
#include "dlstreamer/vaapi/mappers/dma_to_vaapi.h"
#include "dlstreamer/vaapi/mappers/vaapi_to_dma.h"
#include "dlstreamer/vaapi/tensor.h"

#include <va/va.h>
#include <va/va_backend.h>

namespace dlstreamer {

class VAAPIContext;
using VAAPIContextPtr = std::shared_ptr<VAAPIContext>;

class VAAPIContext : public BaseContext {
  public:
    struct key {
        static constexpr auto va_display = BaseContext::key::va_display;
        static constexpr auto va_tile_id = BaseContext::key::va_tile_id;
    };

    static inline VAAPIContextPtr create(const ContextPtr &another_context) {
        // FIXME: Add support for VA only
        return create_from_another<VAAPIContext>(another_context, MemoryType::VAAPI);
    }

    VAAPIContext(void *va_display) : BaseContext(MemoryType::VAAPI) {
        _va_display = va_display;
    }

    VAAPIContext(const ContextPtr &another_context) : BaseContext(MemoryType::VAAPI) {
        DLS_CHECK(another_context);
        DLS_CHECK(_va_display = another_context->handle(key::va_display));
        _parent = another_context;
    }

    VADisplay va_display() {
        return _va_display;
    }

    int get_current_tile_id() const noexcept {
        VADisplayAttribValSubDevice reg;
        VADisplayAttribute reg_attr;
        reg_attr.type = VADisplayAttribType::VADisplayAttribSubDevice;
        VADisplayContextP disp_context = reinterpret_cast<VADisplayContextP>(_va_display);
        auto drv_context = disp_context->pDriverContext;
        auto drv_vtable = *drv_context->vtable;
        if (drv_vtable.vaGetDisplayAttributes(drv_context, &reg_attr, 1) == VA_STATUS_SUCCESS) {
            reg.value = reg_attr.value;
            if (reg.bits.sub_device_count > 0)
                return static_cast<int>(reg.bits.current_sub_device);
        }
        return -1;
    }

    bool is_valid() noexcept {
        static constexpr int _VA_DISPLAY_MAGIC = 0x56414430; // #include <va/va_backend.h>
        return _va_display && (_VA_DISPLAY_MAGIC == *(int *)_va_display);
    }

    std::vector<std::string> keys() const override {
        return {key::va_display};
    }

    void *handle(std::string_view key) const noexcept override {
        if (key == key::va_display || key.empty())
            return _va_display;
        if (key == key::va_tile_id || key.empty())
            return reinterpret_cast<void *>(static_cast<intptr_t>(get_current_tile_id()));
        return nullptr;
    }

    MemoryMapperPtr get_mapper(const ContextPtr &input_context, const ContextPtr &output_context) override {
        auto mapper = BaseContext::get_mapper(input_context, output_context);
        if (mapper)
            return mapper;

        auto input_type = input_context ? input_context->memory_type() : MemoryType::CPU;
        auto output_type = output_context ? output_context->memory_type() : MemoryType::CPU;
        if (input_type == MemoryType::VAAPI && output_type == MemoryType::DMA)
            mapper = std::make_shared<MemoryMapperVAAPIToDMA>(input_context, output_context);
        if (input_type == MemoryType::DMA && output_type == MemoryType::VAAPI)
            mapper = std::make_shared<MemoryMapperDMAToVAAPI>(input_context, output_context);

        BaseContext::attach_mapper(mapper);
        return mapper;
    }

  protected:
    VADisplay _va_display;
};

} // namespace dlstreamer
