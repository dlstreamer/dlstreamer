/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "opencv_barcode_detector.h"
#include "opencv_cropscale.h"
#include "opencv_find_contours.h"
#include "opencv_meta_overlay.h"
#include "opencv_object_association.h"
#include "opencv_remove_background.h"
#include "opencv_tensor_normalize.h"
#include "opencv_warp_affine.h"
#include "tensor_postproc_human_pose.h"

extern "C" {

DLS_EXPORT const dlstreamer::ElementDesc *dlstreamer_elements[] = { //
    &opencv_find_contours,
    &opencv_barcode_detector,
    &opencv_object_association,
    &opencv_tensor_normalize,
    &opencv_cropscale,
    &opencv_meta_overlay,
    &opencv_remove_background,
    &tensor_postproc_human_pose,
#ifdef DLS_HAVE_OPENCV_UMAT
    &opencv_warp_affine,
#endif
    nullptr};
}
