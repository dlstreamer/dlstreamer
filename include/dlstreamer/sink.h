/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/element.h"

namespace dlstreamer {

/**
 * @brief Abstract interface for sink elements. Sink element has one input and no output.
 */
class Sink : public Element {
  public:
    /**
     * @brief Returns input information.
     */
    virtual FrameInfo get_input_info() = 0;

    /**
     * @brief The function notifies element about input information.
     * @param info Output frames information
     */
    virtual void set_input_info(const FrameInfo &info) = 0;

    /**
     * @brief Write one frame
     * @param frame Frame to write
     */
    virtual void write(FramePtr frame) = 0;
};

using SinkPtr = std::shared_ptr<Sink>;

static inline SinkPtr create_sink(const ElementDesc &desc, const AnyMap &params = AnyMap(),
                                  const ContextPtr &app_context = nullptr) {
    Element *element = desc.create(std::make_shared<BaseDictionary>(params), app_context);
    Sink *sink = dynamic_cast<Sink *>(element);
    if (!sink)
        throw std::runtime_error("Error on dynamic_cast<Sink*>");
    return SinkPtr(sink);
}

template <class Ty>
static inline std::unique_ptr<Ty> create_sink(const AnyMap &params = AnyMap(),
                                              const ContextPtr &app_context = nullptr) {
    return std::make_unique<Ty>(std::make_shared<BaseDictionary>(params), app_context);
}

} // namespace dlstreamer
