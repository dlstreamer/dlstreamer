/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvametaconvert.h"
#include "gva_json_meta.h"
#include "gva_tensor_meta.h"
#include "region_of_interest.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void all_to_json(GstGvaMetaConvert *converter, GstBuffer *buffer);
void detection_to_json(GstGvaMetaConvert *converter, GstBuffer *buffer);
void tensor_to_json(GstGvaMetaConvert *converter, GstBuffer *buffer);

#ifdef __cplusplus
} /* extern C */
#endif /* __cplusplus */
