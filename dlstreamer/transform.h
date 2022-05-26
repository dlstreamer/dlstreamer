/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "buffer_mapper.h"
#include "dictionary.h"
#include "dlstreamer/buffer.h"
#include <functional>
#include <string_view>
#include <utility>

namespace dlstreamer {

// FIXME: Is it good name? Options are: dispather, binding, manager, ... more ...
class ITransformController {
  public:
    virtual ~ITransformController() = default;

    virtual ContextPtr get_context(std::string_view name) = 0;

    /**
     * @brief Creates a input mapper object
     *
     * @param buffer_type desired mapped buffer type
     * @param context optional context for mapper instantiation
     * @return BufferMapperPtr smart pointer to created mapper object
     */
    virtual BufferMapperPtr create_input_mapper(BufferType buffer_type, ContextPtr context) = 0;

    /**
     * @brief Creates a input mapper object
     *
     * @param buffer_type desired mapped buffer type
     * @return BufferMapperPtr smart pointer to created mapper object
     */
    BufferMapperPtr create_input_mapper(BufferType buffer_type) {
        return create_input_mapper(buffer_type, {});
    }

    template <class CtxTy>
    std::shared_ptr<CtxTy> get_context() {
        return std::dynamic_pointer_cast<CtxTy>(get_context(CtxTy::context_name));
    }
};

class TransformBase {
  protected:
    ITransformController *_transform_ctrl;
    DictionaryCPtr _params;

  public:
    template <class Ty>
    static std::unique_ptr<TransformBase> create(ITransformController &transform_ctrl, DictionaryCPtr params) {
        return std::make_unique<Ty>(transform_ctrl, std::move(params));
    }

    TransformBase(ITransformController &transform_ctrl, DictionaryCPtr params)
        : _transform_ctrl(&transform_ctrl), _params(std::move(params)) {
    }
    virtual ~TransformBase() = default;

    virtual BufferInfoVector get_input_info(const BufferInfo &output_info) = 0;
    virtual BufferInfoVector get_output_info(const BufferInfo &input_info) = 0;
    virtual void set_info(const BufferInfo &input_info, const BufferInfo &output_info) = 0;
    virtual ContextPtr get_context(const std::string &name) = 0;

    // FIXME: Add flushing mechanism
};

using TransformBasePtr = std::shared_ptr<TransformBase>;

class Transform : public TransformBase {
  public:
    using TransformBase::TransformBase;

    virtual bool process(BufferPtr src, BufferPtr dst) = 0;
};

class TransformWithAlloc : public Transform {
  public:
    using Transform::Transform;

    virtual std::function<BufferPtr()> get_output_allocator() = 0;
    virtual BufferMapperPtr get_output_mapper() = 0;
};

class TransformInplace : public TransformBase {
  public:
    using TransformBase::TransformBase;

    virtual bool process(BufferPtr buffer) = 0;

    BufferInfoVector get_input_info(const BufferInfo &output_info) override {
        return {output_info};
    }
    BufferInfoVector get_output_info(const BufferInfo &input_info) override {
        return {input_info};
    }
    ContextPtr get_context(const std::string & /*name*/) override {
        return {};
    }
};

struct ParamDesc {
    std::string name;
    std::string description;
    Any default_value;
    std::vector<Any> range;

    ParamDesc(std::string_view name, std::string_view desc, Any default_value, std::vector<Any> valid_values = {})
        : name(name), description(desc), default_value(std::move(default_value)), range(std::move(valid_values)) {
    }

    ParamDesc(std::string_view name, std::string_view desc, const Any &default_value, const Any &min_value,
              const Any &max_value)
        : ParamDesc(name, desc, default_value, {min_value, max_value}) {
    }

    // TODO can we use general constructor for string-typed parameters?
    ParamDesc(std::string_view name, std::string_view desc, const char *default_value)
        : name(name), description(desc), default_value(std::string(default_value)) {
    }

    template <typename T>
    bool is_type() const {
        return AnyHoldsType<T>(default_value);
    }
};

using ParamDescVector = std::vector<ParamDesc>;

enum TransformFlags {
    TRANSFORM_FLAG_OUTPUT_ALLOCATOR = (1 << 0),
    TRANSFORM_FLAG_SHARABLE = (1 << 1),
    TRANSFORM_FLAG_MULTISTREAM_MUXER = (1 << 2),
    TRANSFORM_FLAG_SUPPORT_PARAMS_STRUCTURE = (1 << 3),
};

struct TransformDesc {
    std::string_view name;
    std::string_view description;
    std::string_view author;
    ParamDescVector *params;
    BufferInfoVector input_info;
    BufferInfoVector output_info;
    const std::function<std::unique_ptr<TransformBase>(ITransformController &, DictionaryCPtr)> create;
    int flags = 0;
};

} // namespace dlstreamer
