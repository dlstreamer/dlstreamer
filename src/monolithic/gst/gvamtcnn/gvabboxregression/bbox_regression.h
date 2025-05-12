/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once

#include "gstgvabboxregression.h"

G_BEGIN_DECLS

#define SCORE_MAP_INDEX 1
#define PNET_WINDOW_STEP 2
#define PNET_SCAN_WINDOW_SIZE 12
#define PNET_THRESHOLD 0.6
#define RNET_THRESHOLD 0.7
#define ONET_THRESHOLD 0.7
#define RNET_OUT_CONV_NAME "conv5-2"
#define RNET_OUT_PROB_NAME "prob1"
#define ONET_OUT_CONV_NAME "conv6-2"
#define ONET_OUT_FLD_NAME "conv6-3"
#define ONET_OUT_PROB_NAME "prob1"
#define PNET_IN_SIZE(out) ((out + 5) * 2 - 1)

#define OUT_MAP(out, x, y, idx, sx, sy, sz) ((out)[(idx) * (sz) + (x) * (sx) + (y) * (sy)])
#define PROB_MAP(out, x, y, sx, sy, sz) OUT_MAP(out, x, y, SCORE_MAP_INDEX, sx, sy, sz)

gboolean bbox_regression(GstGvaBBoxRegression *bboxregression, GstBuffer *buffer);

G_END_DECLS
