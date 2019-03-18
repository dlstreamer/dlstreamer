/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>
#include <gst/gstmeta.h>
#include <gst/video/gstvideofilter.h>
#include <gst/video/video.h>

#include "inference_backend/image_inference.h"

typedef struct {
    GstBuffer *buffer;
    GstVideoRegionOfInterestMeta roi;
} InferenceFrame;

void Blob2TensorMeta(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &blobs,
                     std::vector<InferenceFrame> frames, const gchar *inference_id, const gchar *model_name);

void Blob2RoiMeta(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &blobs,
                  std::vector<InferenceFrame> frames, const gchar *inference_id, const gchar *model_name,
                  const std::map<std::string, GstStructure *> &model_proc);
