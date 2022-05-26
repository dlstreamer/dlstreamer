/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/gst/shared_transforms.h"

namespace dlstreamer {

MultiValueStorage<TransformBase *, GstBaseTransform *> g_gst_base_transform_storage;

} // namespace dlstreamer
