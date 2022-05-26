/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "i_preproc_elem.hpp"

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>

G_BEGIN_DECLS

GST_DEBUG_CATEGORY_EXTERN(gva_preproc_base_debug_category);
#define GST_DEBUG_CAT_GVA_PREPROC_BASE gva_preproc_base_debug_category

#define GST_TYPE_GVA_PREPROC_BASE (gva_preproc_base_get_type())
#define GVA_PREPROC_BASE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_PREPROC_BASE, GvaPreprocBase))
#define GVA_PREPROC_BASE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_PREPROC_BASE, GvaPreprocBaseClass))
#define GST_IS_GVA_PREPROC_BASE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_PREPROC_BASE))
#define GST_IS_GVA_PREPROC_BASE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_PREPROC_BASE))

typedef struct _GvaPreprocBase {
    GstBaseTransform base;
    class GvaPreprocBasePrivate *impl;

    void set_preproc_elem(IPreProcElem *elem);
} GvaPreprocBase;

typedef struct _GvaPreprocBaseClass {
    GstBaseTransformClass base_class;
} GvaPreprocBaseClass;

GType gva_preproc_base_get_type(void);

G_END_DECLS
