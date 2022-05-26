/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/transform.h"
#include <gst/gst.h>

namespace dlstreamer {

namespace param {
static constexpr auto shared_instance_id = "shared-instance-id";
static constexpr auto params_structure = "params-structure";
static constexpr auto buffer_pool_size = "buffer-pool-size";
}; // namespace param

GST_EXPORT bool register_transform_as_gstreamer(GstPlugin *plugin, const dlstreamer::TransformDesc &desc);

} // namespace dlstreamer
