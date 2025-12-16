/*******************************************************************************
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "utils.h"

#include <fcntl.h>
#include <stdio.h>

using namespace InferenceBackend;

void create_surface(VaDpyWrapper &display, VASurfaceID *p_surface_id, uint32_t width, uint32_t height, uint32_t fourCC,
                    uint32_t format) {
    VASurfaceAttrib surface_attrib;
    surface_attrib.type = VASurfaceAttribPixelFormat;
    surface_attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    surface_attrib.value.type = VAGenericValueTypeInteger;
    surface_attrib.value.value.i = fourCC;

    VA_CALL(display.drvVtable().vaCreateSurfaces2(display.drvCtx(), format, width, height, p_surface_id, 1,
                                                  &surface_attrib, 1));
}

VADisplay va_open_display(int &fd) {
    fd = open("/dev/dri/renderD128", O_RDWR);
    if (fd < 0) {
        throw std::runtime_error("Error opening /dev/dri/renderD128");
    }

    return VaApiLibBinder::get().GetDisplayDRM(fd);
}

VADisplay createVASurface(VASurfaceID &surface_id, int &drm_fd) {

    VaDpyWrapper display = VaDpyWrapper::fromHandle(va_open_display(drm_fd));

    int major_version = 0, minor_version = 0;
    VA_CALL(VaApiLibBinder::get().Initialize(display.raw(), &major_version, &minor_version));

    create_surface(display, &surface_id, 1920, 1080, FourCC::FOURCC_I420, VA_RT_FORMAT_YUV420);

    return display.raw();
}

Image createSurfaceImage(int &fd) {
    Image img;
    img.format = FourCC::FOURCC_I420;
    img.type = MemoryType::VAAPI;

    img.width = 1920;
    img.height = 1080;
    img.size = 3110400;

    img.stride[0] = 1920;
    img.stride[1] = 960;
    img.stride[2] = 960;
    img.stride[3] = 0;

    img.offsets[0] = 0;
    img.offsets[1] = 2073600;
    img.offsets[2] = 2592000;
    img.offsets[3] = 0;

    img.rect.x = 0;
    img.rect.y = 0;
    img.rect.width = img.width;
    img.rect.height = img.height;

    img.va_display = createVASurface(img.va_surface_id, fd);
    return img;
}

Image createEmptyImage() {
    Image img = Image();

    img.format = FourCC::FOURCC_I420;
    img.type = MemoryType::ANY;

    img.width = 1920;
    img.height = 1080;
    img.size = 3110400;

    img.stride[0] = 1920;
    img.stride[1] = 960;
    img.stride[2] = 960;
    img.stride[3] = 0;

    img.offsets[0] = 0;
    img.offsets[1] = 2073600;
    img.offsets[2] = 2592000;
    img.offsets[3] = 0;

    img.rect.x = 0;
    img.rect.y = 0;
    img.rect.width = img.width;
    img.rect.height = img.height;

    return img;
}
