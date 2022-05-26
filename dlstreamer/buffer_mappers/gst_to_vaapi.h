/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"

#include "dlstreamer/buffer_mapper.h"
#include "dlstreamer/gst/buffer.h"
#include "dlstreamer/vaapi/buffer.h"
#include "dlstreamer/vaapi/context.h"

namespace dlstreamer {

class BufferMapperGSTToVAAPI : public BufferMapper {
    constexpr static GstMapFlags GST_MAP_VA = static_cast<GstMapFlags>(GST_MAP_FLAG_LAST << 1);

  public:
    BufferMapperGSTToVAAPI(ContextPtr vaapi_context) : _vaapi_context(vaapi_context) {
    }

    BufferPtr map(BufferPtr src_buffer, AccessMode mode) override {
        auto gst_src_buffer = std::dynamic_pointer_cast<GSTBuffer>(src_buffer);
        if (!gst_src_buffer)
            throw std::runtime_error("Failed to dynamically cast Buffer to GSTBuffer");
        return map(gst_src_buffer, mode);
    }
    VAAPIBufferPtr map(GSTBufferPtr src_buffer, AccessMode /*mode*/) {
        GstBuffer *buffer = src_buffer->gst_buffer();
        GstMapInfo map_info;
        VAAPIBuffer::VASurfaceID va_surface_id;

        GstMapFlags flags = GST_MAP_VA;
        gboolean sts = gst_buffer_map(buffer, &map_info, flags);
        if (!sts) {
            flags = static_cast<GstMapFlags>(flags | GST_MAP_READ);
            sts = gst_buffer_map(buffer, &map_info, flags);
        }
        if (sts) {
            va_surface_id = *reinterpret_cast<uint32_t *>(map_info.data);
            gst_buffer_unmap(buffer, &map_info);
        } else {
            // Try old mechanism via 'qdata'
            va_surface_id = (unsigned int)(size_t)gst_mini_object_get_qdata(&buffer->mini_object,
                                                                            g_quark_from_static_string("VASurfaceID"));
            void *va_display =
                (void *)gst_mini_object_get_qdata(&buffer->mini_object, g_quark_from_static_string("VADisplay"));
            if (!va_display)
                throw std::runtime_error("Couldn't map buffer (VAAPI memory)");
            _vaapi_context = std::make_shared<VAAPIContext>(va_display);
        }

        return std::make_shared<VAAPIBuffer>(va_surface_id, src_buffer->info(), _vaapi_context);
    }

  protected:
    ContextPtr _vaapi_context;
};

} // namespace dlstreamer
