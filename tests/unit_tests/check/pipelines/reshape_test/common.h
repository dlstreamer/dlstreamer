/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "gva_base_inference.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void check_model_input_info(gchar *pipeline_str, const int expected_width, const int expected_height,
                            const int expected_batch_size);

#ifdef __cplusplus
} /* extern C */
#endif /* __cplusplus */
