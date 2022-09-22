/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/dictionary.h"
#include "dlstreamer/base/metadata.h"
#include "dlstreamer/base/tensor.h"
#include "dlstreamer/frame.h"

namespace dlstreamer {

class BaseFrame : public Frame {
    friend class BaseMemoryMapper;

  public:
    BaseFrame(MediaType media_type, Format format, TensorVector tensors)
        : _media_type(media_type), _memory_type(tensors.front()->memory_type()), _format(format), _tensors(tensors) {
        // printf("Created frame %s: %p\n", memory_type_to_string(_memory_type), this);
    }

    BaseFrame(MediaType media_type, Format format, MemoryType memory_type)
        : _media_type(media_type), _memory_type(memory_type), _format(format) {
        // printf("Created frame %s: %p\n", memory_type_to_string(_memory_type), this);
    }

    ~BaseFrame() {
        // printf("Deleted frame %s: %p\n", memory_type_to_string(_memory_type), this);
    }

    size_t num_tensors() const override {
        return _tensors.size();
    }

    TensorPtr tensor(int index) override {
        if (index < 0) {
            if (_tensors.size() != 1)
                throw std::runtime_error("Error accessing multi-tensors frame without tensor index");
            index = 0;
        }
        return _tensors[index];
    }

    iterator begin() noexcept override {
        return _tensors.begin();
    }

    iterator end() noexcept override {
        return _tensors.end();
    }

    MediaType media_type() const override {
        return _media_type;
    }

    Format format() const override {
        return _format;
    }

    MemoryType memory_type() const override {
        return _memory_type;
    }

    Metadata &metadata() override {
        return _metadata;
    }

    FramePtr parent() const override {
        return _parent;
    }

    void set_parent(FramePtr parent) {
        // if (parent) {
        //    printf("set_parent: %s->%s, %p->%p\n", memory_type_to_string(tensor(0)->memory_type()),
        //           memory_type_to_string(parent->tensor(0)->memory_type()), this, parent.get());
        //} else {
        //    printf("set_parent: %s->NULL, %p->NULL\n", memory_type_to_string(tensor(0)->memory_type()), this);
        //    printf("ref_count=%d\n", (int)_parent.use_count());
        //}
        _parent = parent;
        // set parent for all tensors
        for (size_t i = 0; i < num_tensors(); i++) {
            auto base_tensor = std::dynamic_pointer_cast<BaseTensor>(tensor(i));
            if (base_tensor) {
                if (parent && i < parent->num_tensors())
                    base_tensor->set_parent(parent->tensor(i));
                else
                    base_tensor->set_parent(nullptr);
            }
        }
    }

    std::vector<FramePtr> regions() const override {
        return _regions;
    }

    void add_region(FramePtr frame) {
        _regions.push_back(frame);
    }

  protected:
    MediaType _media_type = MediaType::Any;
    MemoryType _memory_type = MemoryType::Any;
    Format _format = 0;
    TensorVector _tensors;
    BaseMetadata _metadata;
    FramePtr _parent;
    std::vector<FramePtr> _regions;
};

using BaseFramePtr = std::shared_ptr<BaseFrame>;

} // namespace dlstreamer
