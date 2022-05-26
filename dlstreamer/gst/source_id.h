/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/context.h"
#include "dlstreamer/dictionary.h"

namespace dlstreamer {

class GSTStreamIdContext : public Context {
  public:
    static constexpr auto context_name = "stream_id";
    static constexpr auto field_name = "stream_id";
};

} // namespace dlstreamer
