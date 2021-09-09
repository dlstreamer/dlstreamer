/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <memory>

// FWD
using VaApiDisplayPtr = std::shared_ptr<void>;

namespace VaapiHelper {

// Sends GstContext query for GstVaapiDisplay and processes the response.
VaApiDisplayPtr queryVaDisplay(GstBaseTransform *element);

} // namespace VaapiHelper
