/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/memory_mapper.h"
#include "dlstreamer/cpu/tensor.h"
#include "dlstreamer/cpu/utils.h"
#include "dlstreamer/gst/allocator.h"
#include "dlstreamer/gst/frame.h"

#define DLS_AVOID_GST_LIBRARY_LINKAGE

namespace dlstreamer {

class MemoryMapperGSTToCPU : public BaseMemoryMapper {
  public:
    using BaseMemoryMapper::BaseMemoryMapper;

    TensorPtr map(TensorPtr src, AccessMode mode) override {
        auto info = src->info();
        auto gst_tensor = ptr_cast<GSTTensor>(src);
        GstMemory *mem = gst_tensor->gst_memory();
        int offset_x = gst_tensor->offset_x();
        int offset_y = gst_tensor->offset_y();

        // If tensor with partial shape and GstMemory allocated by GstDLStreamerAllocator, get TensorPtr and map.
        // Otherwise, use gst_memory_map and create new CPUTensor
        if (!info.size()) {
#ifndef DLS_AVOID_GST_LIBRARY_LINKAGE
            if (gst_is_dlstreamer_memory(mem)) {
                auto tensor = gst_dlstreamer_memory_get_tensor_ptr(mem);
#else
            // direct access to struct GstDLStreamerMemory to avoid libdlstreamer_gst.so linkage
            if (G_TYPE_CHECK_INSTANCE_TYPE(mem->allocator, g_type_from_name(GST_DLSTREAMER_ALLOCATOR_TYPE_NAME))) {
                auto tensor = GST_DLSTREAMER_MEMORY_CAST(mem)->tensor;
#endif
                auto dst = tensor.map(mode);
                if (offset_x || offset_y) {
                    ImageInfo image_info(dst->info());
                    std::vector<std::pair<size_t, size_t>> slice(image_info.info().shape.size());
                    slice[image_info.layout().w_position()] = {offset_x, image_info.width()};
                    slice[image_info.layout().h_position()] = {offset_y, image_info.height()};
                    dst = get_tensor_slice(dst, slice);
                }
                return dst;
            }
        }

        GstMapInfo *map_info = new GstMapInfo;
        DLS_CHECK(gst_memory_map(mem, map_info, mode_to_gst_map_flags(mode)));
        uint8_t *data = map_info->data;

        if (offset_x || offset_y) {
            ImageInfo image_info(src->info());
            data += offset_y * image_info.width_stride() + offset_x * image_info.channels_stride();
        }

        auto dst_tensor = new CPUTensor(info, data);
        auto deleter = [mem, map_info](CPUTensor *tensor) {
            if (map_info) {
                gst_memory_unmap(mem, map_info);
                delete map_info;
            }
            delete tensor;
        };
        auto dst = CPUTensorPtr(dst_tensor, deleter);
        dst->set_parent(src);
        return dst;
    }

    FramePtr map(FramePtr src, AccessMode mode) override {
        auto gst_buffer = ptr_cast<GSTFrame>(src);
        if (gst_buffer->video_info()) { // Video mapped via gst_video_frame_map() single call for all tensors
            return map_video(gst_buffer, mode);
        } else {
            return BaseMemoryMapper::map(src, mode);
        }
    }

  private:
    FramePtr map_video(GstFramePtr src, AccessMode mode) {
        // Add GST_VIDEO_FRAME_MAP_FLAG_NO_REF to not increment/decrement GstBuffer ref-count during map/unmap
        GstMapFlags map_flags = mode_to_gst_map_flags(mode);
        map_flags = static_cast<GstMapFlags>((unsigned)map_flags | (unsigned)GST_VIDEO_FRAME_MAP_FLAG_NO_REF);

        auto frame_ptr = std::make_shared<GstVideoFrame>();
        GstVideoFrame *frame = frame_ptr.get();
        if (!gst_video_frame_map(frame, (GstVideoInfo *)src->video_info(), src->gst_buffer(), map_flags)) {
            throw std::runtime_error("Failed to map GstBuffer to system memory");
        }

        auto n_planes = GST_VIDEO_FRAME_N_PLANES(frame);
        // get GstVideoInfo from GstVideoFrame
        // after gst_video_frame_map, video frame has valid offset and strides obtained from GstVideoMeta of GstBuffer
        auto info = gst_video_info_to_frame_info(&frame->info);
        TensorVector tensors(n_planes);
        for (guint i = 0; i < n_planes; ++i) {
            void *data = GST_VIDEO_FRAME_PLANE_DATA(frame, i);
            if (i < static_cast<guint>(src->num_tensors())) {
                auto src_tensor = ptr_cast<GSTTensor>(src->tensor(i));
                int offset_x = src_tensor->offset_x();
                int offset_y = src_tensor->offset_y();
                if (offset_x || offset_y) {
                    auto stride = GST_VIDEO_FRAME_PLANE_STRIDE(frame, i);
                    data = (uint8_t *)data + offset_y * stride + offset_x;
                }
            }
            tensors[i] = std::make_shared<CPUTensor>(info.tensors[i], data);
        }
        auto dst = new BaseFrame(info.media_type, info.format, tensors);
        auto deleter = [frame_ptr](BaseFrame *dst) {
            gst_video_frame_unmap(frame_ptr.get());
            delete dst;
        };
        auto ret = BaseFramePtr(dst, deleter);
        ret->set_parent(src);
        return ret;
    }

    GstMapFlags mode_to_gst_map_flags(AccessMode mode) {
        int map_flags = 0;
        if (static_cast<int>(mode) & static_cast<int>(AccessMode::Read))
            map_flags |= GST_MAP_READ;
        if (static_cast<int>(mode) & static_cast<int>(AccessMode::Write))
            map_flags |= GST_MAP_WRITE;
        return static_cast<GstMapFlags>(map_flags);
    }
};

} // namespace dlstreamer
