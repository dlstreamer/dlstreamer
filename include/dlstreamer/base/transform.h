/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/element.h"
#include "dlstreamer/base/frame.h"
#include "dlstreamer/base/pool.h"
#include "dlstreamer/transform.h"

#define BUFFER_POOL_SIZE_DEFAULT 16

namespace dlstreamer {

class BaseTransform : public BaseElement<Transform> {
  public:
    BaseTransform(const ContextPtr &app_context) : _app_context(app_context) {
    }

    void set_input_info(const FrameInfo &info) override {
        _input_info = info;
    }

    void set_output_info(const FrameInfo &info) override {
        _output_info = info;
    }

    FrameInfoVector get_input_info() override {
        return {_input_info};
    }

    FrameInfoVector get_output_info() override {
        return {_output_info};
    }

    FramePtr process(FramePtr src) override {
        FramePtr dst = create_output();
        if (process(src, dst))
            return dst;
        else
            return nullptr;
    }

    TensorPtr process(TensorPtr src) override {
        TensorPtr dst = create_output()->tensor();
        if (process(src, dst))
            return dst;
        else
            return nullptr;
    }

    bool process(FramePtr src, FramePtr dst) override {
        return process(src->tensor(), dst->tensor());
    }

    bool process(TensorPtr src, TensorPtr dst) override {
        auto src_frame = std::make_shared<BaseFrame>(MediaType::Tensors, 0, TensorVector({src}));
        auto dst_frame = std::make_shared<BaseFrame>(MediaType::Tensors, 0, TensorVector({dst}));
        return process(src_frame, dst_frame);
    }

    size_t pool_size() {
        return _pool ? _pool->size() : 0;
    }

  protected:
    ContextPtr _app_context;
    FrameInfo _input_info;
    FrameInfo _output_info;
    std::unique_ptr<Pool<FramePtr>> _pool;
    int _buffer_pool_size = BUFFER_POOL_SIZE_DEFAULT;
    std::once_flag _pool_create_once;

    virtual std::function<FramePtr()> get_output_allocator() = 0;

    Pool<FramePtr> *get_pool() {
        std::call_once(_pool_create_once, [this] {
            auto output_allocator = get_output_allocator();
            _pool = std::make_unique<Pool<FramePtr>>(output_allocator, is_frame_available, _buffer_pool_size);
        });
        return _pool.get();
    }

    FramePtr create_output() {
        // get/create frame from frame pool
        FramePtr out = get_pool()->get_or_create();

        // remove previous metadata
        out->metadata().clear();

        return out;
    }

    static bool is_frame_available(FramePtr &frame) {
        // check ref-count of FramePtr, use_count() is std::shared_ptr function
        if (frame.use_count() > 1)
            return false;
        // check ref-count of each TensorPtr
        for (const TensorPtr &tensor : frame) {
            if (tensor.use_count() > 1)
                return false;
        }
        return true;
    }

    std::string_view name() const {
        return typeid(*this).name();
    }
};

using BaseTransformPtr = std::shared_ptr<BaseTransform>;

class BaseTransformInplace : public BaseElement<TransformInplace> {
  public:
    BaseTransformInplace(const ContextPtr &app_context) : _app_context(app_context) {
    }

    void set_info(const FrameInfo &info) override {
        _info = info;
    }

    bool process(FramePtr src) override {
        return process(src->tensor());
    }

    bool process(TensorPtr src) override {
        auto src_frame = std::make_shared<BaseFrame>(MediaType::Tensors, 0, TensorVector({src}));
        return process(src_frame);
    }

  protected:
    ContextPtr _app_context;
    FrameInfo _info;
};

} // namespace dlstreamer
