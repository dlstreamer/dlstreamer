/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvametaconvert.h"
#include "gva_json_meta.h"
#include "gva_tensor_meta.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

gboolean to_json(GstGvaMetaConvert *converter, GstBuffer *buffer);

#ifdef __cplusplus
} /* extern C */
#endif /* __cplusplus */
