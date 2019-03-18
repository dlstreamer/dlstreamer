/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <memory.h>
#include <stdlib.h>

#include "vaapi_utils.h"
#include "vaapi_vpp.h"

namespace InferenceBackend {

using namespace InferenceBackend;

VAAPI_VPP::~VAAPI_VPP() {
    Close();
}

void VAAPI_VPP::Close() {
}

int VAAPI_VPP::Fourcc2RTFormat(int format_fourcc) const {
    switch (format_fourcc) {
#if VA_MAJOR_VERSION >= 1
    case VA_FOURCC_I420:
        return VA_FOURCC_I420;
#endif
    case VA_FOURCC_NV12:
        return VA_RT_FORMAT_YUV420;
    case VA_FOURCC_RGBP:
        return VA_RT_FORMAT_RGBP;
    default:
        return VA_RT_FORMAT_RGB32;
    }
}

VAStatus VAAPI_VPP::Init(VADisplay va_display, int dst_width, int dst_height, int format_fourcc) {
    this->va_display = va_display;
    this->dst_width = dst_width;
    this->dst_height = dst_height;

    /* Create surface/config/context for VPP */
    VASurfaceAttrib surface_attrib;
    surface_attrib.type = VASurfaceAttribPixelFormat;
    surface_attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    surface_attrib.value.type = VAGenericValueTypeInteger;
    surface_attrib.value.value.i = format_fourcc;
    int rtformat = Fourcc2RTFormat(format_fourcc);

    VA_CALL(vaCreateConfig(va_display, VAProfileNone, VAEntrypointVideoProc, 0, 0, &va_config));

    VA_CALL(vaCreateSurfaces(va_display, rtformat, dst_width, dst_height, &va_surface, 1, &surface_attrib, 1));

    VA_CALL(vaCreateContext(va_display, va_config, dst_width, dst_height, VA_PROGRESSIVE, &va_surface, 1, &va_context));

    dst_format.fourcc = (uint32_t)format_fourcc;
    if (format_fourcc == VA_FOURCC_BGRX) {
        dst_format = {.fourcc = (uint32_t)format_fourcc,
                      .byte_order = VA_LSB_FIRST,
                      .bits_per_pixel = 32,
                      .depth = 24,
                      .red_mask = 0xff0000,
                      .green_mask = 0xff00,
                      .blue_mask = 0xff,
                      .alpha_mask = 0,
                      .va_reserved = {}};
    }

    return VA_STATUS_SUCCESS;
}

void VAAPI_VPP::Convert(const Image &src, Image &dst, bool bAllocateDestination) {
    if (!bAllocateDestination) {
        throw std::runtime_error("ERROR: VAAPI_VPP only supports bAllocateDestination==true\n");
    }

    if (this->va_display == 0 || dst.width != dst_width || dst.height != dst_height ||
        dst.format != (int)dst_format.fourcc) {
        Close();
        VA_CALL(Init(src.va_display, dst.width, dst.height, dst.format));
    }

    VAProcPipelineParameterBuffer pipeline_param = {};
    pipeline_param.surface = src.va_surface;
    VARectangle surface_region = {.x = (int16_t)src.rect.x,
                                  .y = (int16_t)src.rect.y,
                                  .width = (uint16_t)src.rect.width,
                                  .height = (uint16_t)src.rect.height};
    if (surface_region.width > 0 && surface_region.height > 0)
        pipeline_param.surface_region = &surface_region;
    pipeline_param.filter_flags = VA_FILTER_SCALING_HQ; // High-quality scaling method

    VABufferID pipeline_param_buf_id = VA_INVALID_ID;
    VA_CALL(vaCreateBuffer(va_display, va_context, VAProcPipelineParameterBufferType, sizeof(pipeline_param), 1,
                           &pipeline_param, &pipeline_param_buf_id));

    VA_CALL(vaBeginPicture(va_display, va_context, va_surface));

    VA_CALL(vaRenderPicture(va_display, va_context, &pipeline_param_buf_id, 1));

    VA_CALL(vaEndPicture(va_display, va_context));

    VA_CALL(vaDestroyBuffer(va_display, pipeline_param_buf_id));

    VA_CALL(vaSyncSurface(va_display, va_surface));

    // Map surface to system memory
    VA_CALL(locker.Map(va_display, va_surface, &dst_format, dst_width, dst_height));
    locker.GetImageBuffer(dst.planes, dst.stride);
}

void VAAPI_VPP::ReleaseImage(const Image &) {
    locker.Unmap();
}

PreProc *CreatePreProcVAAPI() {
    return new VAAPI_VPP();
}

} // namespace InferenceBackend
