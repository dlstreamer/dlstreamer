/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/element.h"

namespace dlstreamer {

/**
 * @brief Abstract interface for source elements. Source element has one output and no input. Source element
 * is responsible for allocating output frame/tensor.
 */
class Source : public Element {
  public:
    /**
     * @brief Returns output information of the source.
     */
    virtual FrameInfo get_output_info() = 0;

    /**
     * @brief Enables post-processing (ex, resize, color-conversion) to specified format and tensor shape.
     * @param info Output frames information
     */
    virtual void set_output_info(const FrameInfo &info) = 0;

    /**
     * @brief Read one frame from the source.
     * @param dst Output frame
     */
    virtual FramePtr read() = 0;
};

using SourcePtr = std::shared_ptr<Source>;

static inline SourcePtr create_source(const ElementDesc &desc, const AnyMap &params = AnyMap(),
                                      const ContextPtr &app_context = nullptr) {
    Element *element = desc.create(std::make_shared<BaseDictionary>(params), app_context);
    Source *source = dynamic_cast<Source *>(element);
    if (!source)
        throw std::runtime_error("Error on dynamic_cast<Source*>");
    return SourcePtr(source);
}

template <class Ty>
static inline std::unique_ptr<Ty> create_source(const AnyMap &params = AnyMap(),
                                                const ContextPtr &app_context = nullptr) {
    return std::make_unique<Ty>(std::make_shared<BaseDictionary>(params), app_context);
}

} // namespace dlstreamer
