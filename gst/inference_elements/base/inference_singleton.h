/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

#ifdef __cplusplus
class InferenceImpl;
#else  /* __cplusplus */
typedef struct InferenceImpl InferenceImpl;
#endif /* __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct _GvaBaseInference;
typedef struct _GvaBaseInference GvaBaseInference;

void registerElement(GvaBaseInference *ovino, GError **error);
InferenceImpl *acquire_inference_instance(GvaBaseInference *ovino, GError **error);
void release_inference_instance(GvaBaseInference *ovino);
GstFlowReturn frame_to_base_inference(GvaBaseInference *ovino, GstBuffer *buf, GstVideoInfo *info);
void base_inference_sink_event(GvaBaseInference *ovino, GstEvent *event);
void flush_inference(GvaBaseInference *ovino);

#ifdef __cplusplus
} /* extern C */
#endif /* __cplusplus */
