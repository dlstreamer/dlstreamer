/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __IDENTIFY_INFERENCE_H__
#define __IDENTIFY_INFERENCE_H__

#include <gst/video/video.h>

struct _GstGvaIdentify;
typedef struct _GstGvaIdentify GstGvaIdentify;

#ifdef __cplusplus

#include "inference_backend/image_inference.h"
#include "reid_gallery.h"
#include "tracker.h"

class Identify {
  public:
    Identify(GstGvaIdentify *ovino);
    ~Identify();
    void ProcessOutput(GstBuffer *buffer, GstVideoInfo *info);

  private:
    int frame_num;
    std::unique_ptr<Tracker> tracker;
    std::unique_ptr<EmbeddingsGallery> gallery;
    GstGvaIdentify *masterGstElement_;
};
#else /* __cplusplus */

typedef struct Identify Identify;

#endif /* __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

GstFlowReturn frame_to_identify(GstGvaIdentify *ovino, GstBuffer *buf, GstVideoInfo *info);
Identify *identifier_new(GstGvaIdentify *ovino);
void identifier_delete(Identify *identifier);

#ifdef __cplusplus
} /* extern C */
#endif /* __cplusplus */

#endif /* __IDENTIFY_INFERENCE_H__ */
