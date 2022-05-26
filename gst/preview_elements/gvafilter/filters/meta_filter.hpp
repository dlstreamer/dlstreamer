/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "ifilter.hpp"

#include <set>
#include <string>

class MetaFilter : public IFilter {
  public:
    MetaFilter(const std::string &object_class_filter);
    ~MetaFilter() final = default;

    void invoke(GstBuffer *buf) final;

  private:
    std::set<std::string> _object_classes;
};
