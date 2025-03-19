/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/transform.h"
#include <gst/gst.h>

extern "C" {

GST_EXPORT gboolean register_element_gst_plugin(const dlstreamer::ElementDesc *element, GstPlugin *plugin);
GST_EXPORT gboolean register_elements_gst_plugin(const dlstreamer::ElementDesc **elements, GstPlugin *plugin);
}
