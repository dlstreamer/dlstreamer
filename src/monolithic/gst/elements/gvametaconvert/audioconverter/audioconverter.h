/*******************************************************************************
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once
#include "gstgvametaconvert.h"
#include "gva_json_meta.h"
#include "gva_tensor_meta.h"

void dump_audio_detection(GstGvaMetaConvert *converter, GstBuffer *buffer);
gboolean convert_audio_meta_to_json(GstGvaMetaConvert *converter, GstBuffer *buffer);