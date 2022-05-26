/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <preproc_base.hpp>

G_BEGIN_DECLS

GST_DEBUG_CATEGORY_EXTERN(gva_preproc_opencv_debug_category);
#define GST_DEBUG_CAT_GVA_PREPROC_OPENCV gva_preproc_opencv_debug_category

#define GST_TYPE_GVA_PREPROC_OPENCV (gva_preproc_opencv_get_type())
#define GVA_PREPROC_OPENCV(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_PREPROC_OPENCV, GvaPreprocOpencv))
#define GVA_PREPROC_OPENCV_CLASS(klass)                                                                                \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_PREPROC_OPENCV, GvaPreprocOpencvClass))
#define GST_IS_GVA_PREPROC_OPENCV(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_PREPROC_OPENCV))
#define GST_IS_GVA_PREPROC_OPENCV_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_PREPROC_OPENCV))

typedef struct _GvaPreprocOpencv {
    GvaPreprocBase base;
    class GvaPreprocOpencvPrivate *impl;
} GvaPreprocOpencv;

typedef struct _GvaPreprocOpencvClass {
    GvaPreprocBaseClass base_class;
} GvaPreprocOpencvClass;

GType gva_preproc_opencv_get_type(void);

G_END_DECLS
