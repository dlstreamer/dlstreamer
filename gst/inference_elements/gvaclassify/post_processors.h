/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <processor_types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern PostProcFunction EXTRACT_CLASSIFICATION_RESULTS;

void copy_buffer_to_structure(GstStructure *structure, const void *buffer, int size);

#ifdef __cplusplus
}
#endif
