/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/memory_mapper.h"
#include "dlstreamer/gst/mappers/any_to_gst.h"
#include "dlstreamer/opencl/tensor.h"

namespace dlstreamer {

class MemoryMapperGSTToOpenCL : public BaseMemoryMapper {
  public:
    using BaseMemoryMapper::BaseMemoryMapper;

    TensorPtr map(TensorPtr src, AccessMode /*mode*/) override {
        auto src_gst = ptr_cast<GSTTensor>(src);
        GstMemory *mem = src_gst->gst_memory();
        GstMapInfo map_info;
        DLS_CHECK(gst_memory_map(mem, &map_info, GST_MAP_NATIVE_HANDLE));
        cl_mem clmem = reinterpret_cast<cl_mem>(map_info.data);
        gst_memory_unmap(mem, &map_info);

        auto dst = std::make_shared<OpenCLTensor>(src->info(), _output_context, clmem);

        int offset_x = src_gst->offset_x();
        int offset_y = src_gst->offset_y();
        if (offset_x || offset_y) {
            ImageInfo image_info(src->info());
            size_t offset = offset_y * image_info.width_stride() + offset_x * image_info.channels_stride();
            dst->set_handle(tensor::key::offset, offset);
        }

        dst->set_parent(src);
        return dst;
    }
};

} // namespace dlstreamer
