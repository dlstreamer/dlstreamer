/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/metadata.h"
#include "dlstreamer/tensor.h"

#include <assert.h>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace dlstreamer {

enum class MediaType { Any = 0, Tensors = 1, Image = 2, Audio = 3 };

// Media-type specific format: ImageFormat for MediaType::Image, AudioFormat for MediaType::Audio
typedef int64_t Format;

class FramePtr;

/**
 * @brief Frame is collection of one or multiple Tensor objects, attributes (media type and format), and optional
 * metadata as container of Dictionary objects.
 */
class Frame {
  public:
    Frame() = default;
    Frame(const Frame &) = delete;
    Frame &operator=(const Frame &) = delete;
    virtual ~Frame() {
    }

    /**
     * @brief Returns media type (Tensors, Image, Audio, etc).
     */
    virtual MediaType media_type() const = 0;

    /**
     * @brief Returns format (media type specific) of frame's data.
     */
    virtual Format format() const = 0;

    /**
     * @brief Returns memory type used for tensors allocation.
     */
    virtual MemoryType memory_type() const = 0;

    // access to tensor list
    using iterator = std::vector<TensorPtr>::iterator;
    virtual iterator begin() noexcept = 0;
    virtual iterator end() noexcept = 0;

    /**
     * @brief Returns tensor by index. If index less than 0 (default is -1), function checks that frame contains only
     * one tensor and returns it, otherwise exception is thrown.
     * @param index Tensor index
     */
    virtual TensorPtr tensor(int index = -1) = 0;

    /**
     * @brief Returns number tensors in frame.
     */
    virtual size_t num_tensors() const = 0;

    /**
     * @brief Returns metadata object.
     */
    virtual Metadata &metadata() = 0;

    /**
     * @brief Returns metadata object.
     */
    const Metadata &metadata() const {
        return const_cast<Frame *>(this)->metadata();
    }

    /**
     * @brief Returns parent frame if this frame was mapped (using MemoryMapper) from another frame or contains
     * sub-region of another frame, otherwise returns nullptr.
     */
    virtual FramePtr parent() const = 0;

    /**
     * @brief Returns list of regions. Each region typically represents an object detected on frame and may contain
     * own metadata describing region specific attributes.
     */
    virtual std::vector<FramePtr> regions() const = 0;
};

class FramePtr : public std::shared_ptr<Frame> {
  public:
    using std::shared_ptr<Frame>::shared_ptr; // inherit constructors

    FramePtr map(const ContextPtr &output_context, AccessMode access_mode = AccessMode::ReadWrite);

    FramePtr map(AccessMode access_mode = AccessMode::ReadWrite) {
        return map(nullptr, access_mode);
    }

    template <typename T>
    inline std::shared_ptr<T> map(const ContextPtr &output_context, AccessMode access_mode = AccessMode::ReadWrite) {
        return ptr_cast<T>(map(output_context, access_mode));
    }

    inline Frame::iterator begin() noexcept {
        return get()->begin();
    }
    inline Frame::iterator end() noexcept {
        return get()->end();
    }
};

//////////////////////////////////////////////////////////////
// FramePtr::map

inline FramePtr FramePtr::map(const ContextPtr &output_context, AccessMode access_mode) {
    auto input_context = get()->tensor(0)->context(); // TODO tensor(0)
    if (input_context == output_context)
        return *this;
    MemoryMapperPtr mapper = nullptr;
    if (output_context)
        mapper = output_context->get_mapper(input_context, output_context);
    if (!mapper && input_context)
        mapper = input_context->get_mapper(input_context, output_context);
    if (!mapper) {
        auto to_string = [](ContextPtr context) {
            MemoryType memory_type = context ? context->memory_type() : MemoryType::CPU;
            return std::string(memory_type_to_string(memory_type));
        };
        throw std::runtime_error("Error getting mapper from " + to_string(input_context) + " to " +
                                 to_string(output_context));
    }
    return mapper->map(*this, access_mode);
}

} // namespace dlstreamer
