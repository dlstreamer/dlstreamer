/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/vaapi/elements/vaapi_sync.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/utils.h"
#include "dlstreamer/vaapi/context.h"

#include <mutex>
#include <va/va_backend.h>

namespace dlstreamer {

namespace param {

static constexpr auto timeout = "timeout";
static constexpr auto default_timeout = 10.0; // TODO

}; // namespace param

static ParamDescVector params_desc = {
    {param::timeout, "Synchronization timeout (seconds)", param::default_timeout, 0.0, 1e10}};

class VAAPISync : public BaseTransformInplace {
  public:
    VAAPISync(DictionaryCPtr params, const ContextPtr &app_context) : BaseTransformInplace(app_context) {
        _timeout = static_cast<uint64_t>(params->get<double>(param::timeout, param::default_timeout) * 1e9);
    }

    bool init_once() override {
        _vaapi_context = VAAPIContext::create(_app_context);
        auto _va_display = reinterpret_cast<VADisplayContextP>(_vaapi_context->va_display());
        _va_driver = _va_display->pDriverContext;
        _va_vtable = _va_display->pDriverContext->vtable;

        create_mapper({_app_context, _vaapi_context});
        return true;
    }

    bool process(FramePtr frame) override {
        DLS_CHECK(init());
        auto vaapi_frame = frame.map(_vaapi_context, AccessMode::Read);
        auto va_surface = ptr_cast<VAAPITensor>(vaapi_frame->tensor(0))->va_surface();
        if (_timeout > 0) {
#if VA_CHECK_VERSION(1, 15, 0)
            DLS_CHECK(_va_vtable->vaSyncSurface2(_va_driver, va_surface, _timeout) == VA_STATUS_SUCCESS);
#else
            throw std::runtime_error("vaSyncSurface2 requires VAAPI version >= 1.15");
#endif
        } else {
            DLS_CHECK(_va_vtable->vaSyncSurface(_va_driver, va_surface) == VA_STATUS_SUCCESS);
        }
        return true;
    }

  protected:
    uint64_t _timeout = 0;
    VAAPIContextPtr _vaapi_context;
    VADriverContextP _va_driver = nullptr;
    VADriverVTable *_va_vtable = nullptr;
};

extern "C" {
ElementDesc vaapi_sync = {.name = "vaapi_sync",
                          .description = "Synchronize VAAPI surfaces (call vaSyncSurface)",
                          .author = "Intel Corporation",
                          .params = &params_desc,
                          .input_info = MAKE_FRAME_INFO_VECTOR({{MediaType::Image, MemoryType::VAAPI}}),
                          .output_info = MAKE_FRAME_INFO_VECTOR({{MediaType::Image, MemoryType::VAAPI}}),
                          .create = create_element<VAAPISync>,
                          .flags = 0};
}

} // namespace dlstreamer
