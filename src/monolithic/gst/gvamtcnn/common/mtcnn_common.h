/*******************************************************************************
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef MTCNN_COMMON_H
#define MTCNN_COMMON_H

#include "glib.h"
#include <gst/gst.h>
#include <stdint.h>

#include "utils.h"

G_BEGIN_DECLS

typedef struct {
    int32_t valid;
    int32_t id;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    double score;
    int32_t left_eye_x;
    int32_t left_eye_y;
    int32_t right_eye_x;
    int32_t right_eye_y;
    int32_t nose_x;
    int32_t nose_y;
    int32_t mouth_left_x;
    int32_t mouth_left_y;
    int32_t mouth_right_x;
    int32_t mouth_right_y;
} FaceCandidate;

typedef enum { MODE_PNET, MODE_RNET, MODE_ONET } GstMTCNNModeType;

#define GST_TYPE_MTCNN_MODE (gst_mtcnn_get_mode_type())
GType gst_mtcnn_get_mode_type();

const gchar *mode_type_to_string(GstMTCNNModeType mode);

gboolean foreach_meta_remove_one(GstBuffer *buffer, GstMeta **meta, gpointer to_remove);

G_END_DECLS

#endif /* MTCNN_COMMON_H */
