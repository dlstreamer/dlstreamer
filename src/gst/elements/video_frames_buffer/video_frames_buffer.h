/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef _video_frames_buffer_H_
#define _video_frames_buffer_H_

#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define VIDEO_FRAMES_BUFFER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), video_frames_buffer_get_type(), VideoFramesBuffer))

typedef struct _VideoFramesBuffer VideoFramesBuffer;
typedef struct _VideoFramesBufferClass VideoFramesBufferClass;

struct _VideoFramesBuffer {
    GstBaseTransform base_transform;
    int number_input_frames;
    int number_output_frames;

    /* private */
    GstBuffer **buffers;
    gint curr_input_frames;
    gint curr_output_frames;
    GstClockTime last_pts;
    GstClockTime pts_delta;
};

struct _VideoFramesBufferClass {
    GstBaseTransformClass base_transform_class;
};

GType video_frames_buffer_get_type(void);

G_END_DECLS

#endif
