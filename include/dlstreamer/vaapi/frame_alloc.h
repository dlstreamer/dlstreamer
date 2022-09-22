/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/vaapi/context.h"
#include "dlstreamer/vaapi/frame.h"
#include <va/va.h>

namespace dlstreamer {

#define VA_CALL(_FUNC)                                                                                                 \
    {                                                                                                                  \
        /*ITT_TASK(#_FUNC);*/                                                                                          \
        VAStatus _status = _FUNC;                                                                                      \
        if (_status != VA_STATUS_SUCCESS) {                                                                            \
            throw std::runtime_error(#_FUNC " failed, sts=" + std::to_string(_status));                                \
        }                                                                                                              \
    }

// Creates and controls lifetime of VASurface
class VAAPIFrameAlloc final : public VAAPIFrame {
  public:
    VAAPIFrameAlloc(const FrameInfo &info, ContextPtr context)
        : VAAPIFrame(create_surface(info, context), info, context) {
    }

    ~VAAPIFrameAlloc() {
        VASurfaceID va_surface = VAAPIFrame::va_surface();
        _va_driver->vtable->vaDestroySurfaces(_va_driver, &va_surface, 1);
    }

  private:
    VASurfaceID create_surface(const FrameInfo &info, ContextPtr context) {
        auto va_display = context->handle(BaseContext::key::va_display);
        DLS_CHECK(va_display);
        _va_driver = reinterpret_cast<VADisplayContextP>(va_display)->pDriverContext;

        auto format = video_format_to_vaapi(static_cast<ImageFormat>(info.format));
        auto rt_format = vaapi_video_format_to_rtformat(format);
        ImageInfo image_info(info.tensors.front());

        unsigned int va_surface = VA_INVALID_SURFACE;
        VASurfaceAttrib surface_attr = {};
        surface_attr.type = VASurfaceAttribPixelFormat;
        surface_attr.flags = VA_SURFACE_ATTRIB_SETTABLE;
        surface_attr.value.type = VAGenericValueTypeInteger;
        surface_attr.value.value.i = format;
        VA_CALL(_va_driver->vtable->vaCreateSurfaces2(_va_driver, rt_format, image_info.width(), image_info.height(),
                                                      &va_surface, 1, &surface_attr, 1));
        return va_surface;
    }

    VADriverContext *_va_driver;
};

} // namespace dlstreamer
