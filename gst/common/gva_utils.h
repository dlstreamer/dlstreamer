/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef GVA_UTILS_H
#define GVA_UTILS_H

#include "glib.h"
#include <gst/gst.h>

#ifdef __cplusplus
#include <exception>
#include <gst/video/gstvideometa.h>
#include <inference_backend/image_inference.h>
#include <string>

std::string CreateNestedErrorMsg(const std::exception &e, int level = 0);
std::vector<std::string> SplitString(const std::string &input, char delimiter = ',');
int GetUnbatchedSizeInBytes(InferenceBackend::OutputBlob::Ptr blob, size_t batch_size);
bool file_exists(const std::string &path);

#endif // __cplusplus

G_BEGIN_DECLS

gboolean get_object_id(GstVideoRegionOfInterestMeta *meta, int *id);
void set_object_id(GstVideoRegionOfInterestMeta *meta, gint id);

G_END_DECLS

#endif // GVA_UTILS_H
