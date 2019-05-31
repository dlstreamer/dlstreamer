/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "glib.h"
#include <gst/gst.h>

#ifndef GVA_UTILS_H
#define GVA_UTILS_H

#ifdef __cplusplus
#include <exception>
#include <inference_backend/image_inference.h>
#include <string>

std::string CreateNestedErrorMsg(const std::exception &e, int level = 0);

#endif // __cplusplus

#endif // GVA_UTILS_H
