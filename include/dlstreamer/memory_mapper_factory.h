/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <dlstreamer/base/context.h>
#include <dlstreamer/base/memory_mapper.h>
#include <dlstreamer/context.h>
#include <dlstreamer/utils.h>
#include <list>
#include <numeric>

namespace dlstreamer {

class MemoryMapperChain final : public MemoryMapper {
  public:
    MemoryMapperChain(std::initializer_list<MemoryMapperPtr> l) : _chain(l) {
    }

    MemoryMapperChain(const std::vector<MemoryMapperPtr> &v) : _chain(v) {
        for (auto &mapper : _chain) {
            DLS_CHECK(mapper)
        }
    }

    dlstreamer::TensorPtr map(dlstreamer::TensorPtr src, dlstreamer::AccessMode mode) override {
        return std::accumulate(
            _chain.begin(), _chain.end(), std::move(src),
            [mode](TensorPtr tensor, MemoryMapperPtr mapper) { return mapper->map(std::move(tensor), mode); });
    }

    dlstreamer::FramePtr map(dlstreamer::FramePtr src, dlstreamer::AccessMode mode) override {
        return std::accumulate(
            _chain.begin(), _chain.end(), std::move(src),
            [mode](FramePtr frame, MemoryMapperPtr mapper) { return mapper->map(std::move(frame), mode); });
    }

    ContextPtr input_context() const override {
        return _chain.front()->input_context();
    }

    ContextPtr output_context() const override {
        return _chain.back()->output_context();
    }

  protected:
    std::vector<MemoryMapperPtr> _chain;
};

class MemoryMapperCache final : public MemoryMapper {
  public:
    MemoryMapperCache(MemoryMapperPtr mapper) : _mapper(mapper) {
        DLS_CHECK(mapper);
    }

    TensorPtr map(TensorPtr src, dlstreamer::AccessMode mode) override {
        auto handle = src->handle();
        auto value = _tensors_cache.find(handle);
        // std::string mem1 = memory_type_to_string(_mapper->input_context()->memory_type());
        // std::string mem2 = memory_type_to_string(_mapper->output_context()->memory_type());
        if (value != _tensors_cache.end()) {
            // printf("- using cache %s->%s, %p->%p\n", mem1.data(), mem2.data(), (void*)handle, value->second.get());
            return value->second;
        } else {
            auto dst = _mapper->map(src, mode);
            // printf("- new map %s->%s, %p->%p\n", mem1.data(), mem2.data(), (void*)handle, dst.get());
            auto dst_casted = std::dynamic_pointer_cast<BaseTensor>(dst);
            if (dst_casted)
                dst_casted->set_parent(nullptr);
            _tensors_cache[handle] = dst;
            return dst;
        }
    }

    FramePtr map(FramePtr src, dlstreamer::AccessMode mode) override {
        auto tensor0 = src->tensor(0);
        auto handle = tensor0->handle();
        auto value = _frames_cache.find(handle);
        if (value != _frames_cache.end()) {
            value->second->metadata().clear(); // remove all metadata
            // printf("Re-using frame %s\n", memory_type_to_string(value->second->memory_type()));
            return value->second;
        } else {
            auto dst = _mapper->map(src, mode);
            auto dst_casted = std::dynamic_pointer_cast<BaseFrame>(dst);
            if (dst_casted)
                dst_casted->set_parent(nullptr);
            _frames_cache[handle] = dst;
            return dst;
        }
    }

    ContextPtr input_context() const override {
        return _mapper->input_context();
    }

    ContextPtr output_context() const override {
        return _mapper->output_context();
    }

  private:
    MemoryMapperPtr _mapper;
    std::map<Tensor::handle_t, TensorPtr> _tensors_cache;
    std::map<Tensor::handle_t, FramePtr> _frames_cache;
};

/**
 * @brief Creates chain of memory mappers as requested by vector of ContextPtr objects, and returns mapper object
 * with input context equal to first element in specified vector and output context equal to last element in specified
 * vector of context objects.
 * @param context_chain Vector of context objects defining mapping sequence
 * @param use_cache If true, the returned mapper cashes internally all mapped TensorPtr and FramePtr objects to avoid
 * mapping operation on same TensorPtr/FramePtr multiple times. This optimization is useful for case mapper works on
 * pool of limited number TensorPtr/FramePtr objects.
 */
static inline MemoryMapperPtr create_mapper(std::vector<ContextPtr> context_chain, bool use_cache = false) {
    DLS_CHECK(context_chain.size() >= 2)
    if (context_chain.size() == 2 && context_chain[0] == context_chain[1])
        return std::make_shared<BaseMemoryMapper>(context_chain[0], context_chain[1]);

    std::vector<MemoryMapperPtr> mappers(context_chain.size() - 1);
    for (size_t i = 0; i < mappers.size(); i++) {
        DLS_CHECK(context_chain[i])
        DLS_CHECK(context_chain[i + 1])

        MemoryMapperPtr mapper;
        for (size_t ii = i; ii <= i + 1; ii++) {
            mapper = context_chain[ii]->get_mapper(context_chain[i], context_chain[i + 1]);
            if (mapper)
                break;
        }
        if (!mapper) {
            std::string mem1 = std::string(memory_type_to_string(context_chain[i]->memory_type()));
            std::string mem2 = std::string(memory_type_to_string(context_chain[i + 1]->memory_type()));
            throw std::runtime_error("Can't create mapper from " + mem1 + " to " + mem2);
        }

        mappers[i] = mapper;
    }
    std::string mem1 = std::string(memory_type_to_string(context_chain.front()->memory_type()));
    std::string mem2 = std::string(memory_type_to_string(context_chain.back()->memory_type()));
    // printf("Created mapper from %s to %s, %p -> %p\n", mem1.data(), mem2.data(), context_chain.front().get(),
    //       context_chain.back().get());

    MemoryMapperPtr mapper_chain = std::make_shared<MemoryMapperChain>(mappers);
    if (use_cache)
        mapper_chain = std::make_shared<MemoryMapperCache>(mapper_chain);

    auto input_context = std::dynamic_pointer_cast<BaseContext>(context_chain.front());
    if (input_context)
        input_context->attach_mapper(mapper_chain);
    auto output_context = std::dynamic_pointer_cast<BaseContext>(context_chain.back());
    if (output_context)
        output_context->attach_mapper(mapper_chain);

    return mapper_chain;
}

} // namespace dlstreamer
