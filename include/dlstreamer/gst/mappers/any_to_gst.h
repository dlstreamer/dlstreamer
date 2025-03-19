/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/memory_mapper.h"
#include "dlstreamer/dma/tensor.h"
#include "dlstreamer/gst/allocator.h"
#include <gst/allocators/gstdmabuf.h>
#include <gst/gst.h>

namespace dlstreamer {

class MemoryMapperAnyToGST : public BaseMemoryMapper {
  public:
    using BaseMemoryMapper::BaseMemoryMapper;

    MemoryMapperAnyToGST(const ContextPtr &input_context, const ContextPtr &output_context, bool use_cache)
        : BaseMemoryMapper(input_context, output_context), _use_cache(use_cache) {
    }

    ~MemoryMapperAnyToGST() {
        if (_allocator)
            gst_object_unref(_allocator);
        if (_use_cache) { // call free() on all cached GstBuffer
            for (auto &it : _cache) {
                GstMiniObject *mini_object = &it.second->gst_buffer()->mini_object;
                if (mini_object->refcount == 0) {
                    mini_object->free(mini_object);
                } else {
                    GST_ERROR("Calling free() on GstBuffer with non-zero refcount");
                }
            }
        }
    }

    TensorPtr map(TensorPtr src, AccessMode /*mode*/) override {
        GstMemory *mem = nullptr;
        if (src->memory_type() == MemoryType::DMA) { // use standard GStreamer allocator for memory:DMABuf
            if (!_allocator)
                DLS_CHECK(_allocator = gst_dmabuf_allocator_new());
            auto dma_fd = ptr_cast<DMATensor>(src)->dma_fd();
            auto size = src->info().nbytes();
            mem = gst_dmabuf_allocator_alloc_with_flags(_allocator, dma_fd, size, GST_FD_MEMORY_FLAG_DONT_CLOSE);
        } else { // use custom GstAllocator
            if (!_allocator)
                DLS_CHECK(_allocator = gst_dlstreamer_allocator_new(src->memory_type()));
            mem = gst_dlstreamer_allocator_wrap_tensor(_allocator, src);
        }
        auto dst = std::make_shared<GSTTensor>(src->info(), mem, true, _output_context);
        dst->set_parent(src);
        return dst;
    }

    FramePtr map(FramePtr src, AccessMode mode) override {
        GstFramePtr dst;
        if (_use_cache) {
            auto handle = reinterpret_cast<Tensor::handle_t>(src.get()); // TODO
            auto value = _cache.find(handle);
            if (value != _cache.end()) {
                dst = value->second;
                GstBuffer *buf = dst->gst_buffer();
                // increase ref-count from 0 to 1
                DLS_CHECK(buf->mini_object.refcount == 0)
                gst_buffer_ref(buf);
            } else {
                TensorVector tensors;
                for (auto &tensor : src)
                    tensors.push_back(map(tensor, mode));
                dst = std::make_shared<GSTFrame>(src->media_type(), src->format(), tensors, false);
                _cache[handle] = dst;
            }
            // capture FramePtr in qdata and set dispose function for GstBuffer
            GstMiniObject *mini_object = &dst->gst_buffer()->mini_object;
            mini_object->dispose = buffer_dispose_callback;
            FramePtr *src_ptr = new FramePtr(src);
            gst_mini_object_set_qdata(mini_object, g_quark_from_string(_quark_name), src_ptr, NULL);
        } else {
            TensorVector tensors;
            for (auto &tensor : src)
                tensors.push_back(map(tensor, mode));
            dst = std::make_shared<GSTFrame>(src->media_type(), src->format(), tensors, false);
            // capture FramePtr in qdata and set destroy function for qdata
            GstMiniObject *mini_object = &dst->gst_buffer()->mini_object;
            FramePtr *src_ptr = new FramePtr(src);
            gst_mini_object_set_qdata(mini_object, g_quark_from_string(_quark_name), src_ptr, qdata_destroy_callback);
        }

        // copy metadata
        copy_metadata(*src, *dst);

        return dst;
    }

  protected:
    GstAllocator *_allocator = nullptr;
    bool _use_cache = false;
    std::map<Tensor::handle_t, GstFramePtr> _cache;
    static auto constexpr _quark_name = "FramePtr";

    static void qdata_destroy_callback(gpointer data) {
        FramePtr *src_ptr = reinterpret_cast<FramePtr *>(data);
        if (src_ptr)
            delete src_ptr;
    }

    static gboolean buffer_dispose_callback(GstMiniObject *obj) {
        GstBuffer *buf = reinterpret_cast<GstBuffer *>(obj);
        // remove captured FramePtr
        auto quark = g_quark_from_string(_quark_name);
        FramePtr *src_ptr = reinterpret_cast<FramePtr *>(gst_mini_object_get_qdata(&buf->mini_object, quark));
        if (src_ptr)
            delete src_ptr;

        // remove meta
        gpointer state = NULL;
        while (GstMeta *meta = gst_buffer_iterate_meta(buf, &state)) {
            gst_buffer_remove_meta(buf, meta);
        }

        // return FALSE to keep GstBuffer alive in _cache
        return FALSE;
    }
};

} // namespace dlstreamer
