/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <preproc_base.hpp>

G_BEGIN_DECLS

GST_DEBUG_CATEGORY_EXTERN(gva_preproc_vaapi_debug_category);
#define GST_DEBUG_CAT_GVA_PREPROC_VAAPI gva_preproc_vaapi_debug_category

#define GST_TYPE_GVA_PREPROC_VAAPI (gva_preproc_vaapi_get_type())
#define GVA_PREPROC_VAAPI(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_PREPROC_VAAPI, GvaPreprocVaapi))
#define GVA_PREPROC_VAAPI_CLASS(klass)                                                                                 \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_PREPROC_VAAPI, GvaPreprocVaapiClass))
#define GST_IS_GVA_PREPROC_VAAPI(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_PREPROC_VAAPI))
#define GST_IS_GVA_PREPROC_VAAPI_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_PREPROC_VAAPI))

typedef struct _GvaPreprocVaapi {
    GvaPreprocBase base;
    class GvaPreprocVaapiPrivate *impl;
} GvaPreprocVaapi;

typedef struct _GvaPreprocVaapiClass {
    GvaPreprocBaseClass base_class;
} GvaPreprocVaapiClass;

GType gva_preproc_vaapi_get_type(void);

G_END_DECLS
