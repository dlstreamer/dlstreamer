/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/dictionary.h"
#include "dlstreamer/metadata.h"

namespace dlstreamer {

class BaseMetadata : public Metadata {
    std::vector<DictionaryPtr> _vec;

  public:
    void clear() noexcept override {
        _vec.clear();
    }

    iterator begin() noexcept override {
        return _vec.begin();
    }

    iterator end() noexcept override {
        return _vec.end();
    }

    iterator erase(iterator pos) override {
        return _vec.erase(pos);
    }

    DictionaryPtr add(std::string_view name) override {
        auto item = std::make_shared<BaseDictionary>(name);
        _vec.push_back(item);
        return item;
    }
};

} // namespace dlstreamer
