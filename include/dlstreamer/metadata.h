/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <algorithm>
#include <vector>

#include "dlstreamer/dictionary.h"

namespace dlstreamer {

class Metadata {
  public:
    using iterator = std::vector<DictionaryPtr>::iterator;

    virtual ~Metadata() = default;

    virtual iterator begin() noexcept = 0;
    virtual iterator end() noexcept = 0;
    iterator begin() const noexcept {
        return const_cast<Metadata *>(this)->begin();
    }
    iterator end() const noexcept {
        return const_cast<Metadata *>(this)->end();
    }

    virtual DictionaryPtr add(std::string_view name) = 0;

    virtual iterator erase(iterator pos) = 0;

    virtual iterator erase(iterator first, iterator last) {
        while (first != last) {
            first = erase(first);
        }
        return last;
    }

    virtual void clear() noexcept = 0;
};

} // namespace dlstreamer
