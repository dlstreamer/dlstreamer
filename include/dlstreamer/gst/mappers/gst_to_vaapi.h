/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/memory_mapper.h"
#include "dlstreamer/gst/frame.h"
#include "dlstreamer/vaapi/context.h"
#include "dlstreamer/vaapi/frame.h"

namespace dlstreamer {

class MemoryMapperGSTToVAAPI : public BaseMemoryMapper {
    constexpr static GstMapFlags GST_MAP_VA = static_cast<GstMapFlags>(GST_MAP_FLAG_LAST << 1);

  public:
    using BaseMemoryMapper::BaseMemoryMapper;

    TensorPtr map(TensorPtr src, AccessMode /*mode*/) override {
        auto src_gst = ptr_cast<GSTTensor>(src);
        auto va_surface_id = get_surface_id(src_gst->gst_memory());

        auto ret = std::make_shared<VAAPITensor>(va_surface_id, src_gst->plane_index(), src->info(), _output_context);

        ret->set_handle(tensor::key::offset_x, src_gst->offset_x());
        ret->set_handle(tensor::key::offset_y, src_gst->offset_y());
        ret->set_parent(src);
        return ret;
    }

  protected:
    VAAPITensor::VASurfaceID get_surface_id(GstMemory *mem) {
        GstMapInfo map_info;

        GstMapFlags flags = GST_MAP_VA;
        gboolean sts = gst_memory_map(mem, &map_info, flags);
        if (!sts) { // try with GST_MAP_READ
            flags = static_cast<GstMapFlags>(flags | GST_MAP_READ);
            DLS_CHECK(gst_memory_map(mem, &map_info, flags));
        }
        auto va_surface_id = *reinterpret_cast<VAAPITensor::VASurfaceID *>(map_info.data);
        gst_memory_unmap(mem, &map_info);
        return va_surface_id;
    }
};

} // namespace dlstreamer
