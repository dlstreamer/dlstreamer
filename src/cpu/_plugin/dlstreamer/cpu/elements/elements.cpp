/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "rate_adjust.h"
#include "tensor_convert.h"
#include "tensor_histogram.h"
#include "tensor_sliding_aggregate.h"
// tensor_postproc
#include "tensor_postproc_copy_params.h"
#include "tensor_postproc_detection_output.h"
#include "tensor_postproc_label.h"
#include "tensor_postproc_text.h"
#include "tensor_postproc_yolo_v2.h"

extern "C" {

DLS_EXPORT const dlstreamer::ElementDesc *dlstreamer_elements[] = { //
    &rate_adjust, &tensor_convert, &tensor_histogram, &tensor_sliding_aggregate,
    // post-processing for object detection
    &tensor_postproc_copy_params, &tensor_postproc_detection_output, &tensor_postproc_yolo_v2,
    // post-processing for object classification
    &tensor_postproc_label, &tensor_postproc_text,
    //
    nullptr};
}
