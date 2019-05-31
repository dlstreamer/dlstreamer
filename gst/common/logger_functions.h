/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once
#include <gst/gst.h>

void GST_logger(int level, const char *file, const char *function, int line, const char *message);
