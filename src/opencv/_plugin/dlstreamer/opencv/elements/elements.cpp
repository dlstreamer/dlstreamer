/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "find_contours_opencv.h"
#include "object_association_opencv.h"
#include "remove_background_opencv.h"
#include "tensor_normalize_opencv.h"
#include "video_cropscale_opencv.h"
#include "warp_affine_opencv.h"
#include "watermark_opencv.h"

extern "C" {

DLS_EXPORT const dlstreamer::ElementDesc *dlstreamer_elements[] = { //
    &find_contours_opencv,
    &object_association_opencv,
    &tensor_normalize_opencv,
    &video_cropscale_opencv,
    &watermark_opencv,
    &remove_background_opencv,
#ifdef DLS_HAVE_OPENCV_UMAT
    &warp_affine_opencv,
#endif
    nullptr};
}
