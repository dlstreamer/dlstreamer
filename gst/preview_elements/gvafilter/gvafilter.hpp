/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "filters/ifilter.hpp"

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <utils.h>

#include <memory>

G_BEGIN_DECLS

#define GVA_FILTER_NAME "[Preview] Generic Filter Element"
#define GVA_FILTER_DESCRIPTION "Performs filtering on input buffer data"

GST_DEBUG_CATEGORY_EXTERN(gva_filter_debug_category);
#define GST_DEBUG_CAT_GVA_FILTER gva_filter_debug_category

#define GST_TYPE_GVA_FILTER (gva_filter_get_type())
#define GVA_FILTER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_FILTER, GvaFilter))
#define GVA_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_FILTER, GvaFilterClass))
#define GST_IS_GVA_FILTER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_FILTER))
#define GST_IS_GVA_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_FILTER))

enum FilterType { FILTER_TYPE_META };

typedef struct _GvaFilter {
    GstBaseTransform base;

    struct _Props {
        /* public properties */
        FilterType type;
        std::string object_class_filter;

        /* private properties */
        std::unique_ptr<IFilter> filter;
    } props;
} GvaFilter;

typedef struct _GvaFilterClass {
    GstBaseTransformClass base_class;
} GvaFilterClass;

GType gva_filter_get_type(void);

G_END_DECLS
