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
std::vector<std::string> SplitString(const std::string &input, char delimiter = ',');
int GetUnbatchedSizeInBytes(InferenceBackend::OutputBlob::Ptr blob, size_t batch_size);
bool file_exists(const std::string &path);

#endif // __cplusplus

#endif // GVA_UTILS_H
