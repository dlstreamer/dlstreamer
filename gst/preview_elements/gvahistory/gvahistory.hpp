/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "algorithms/ihistory.hpp"

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <utils.h>

#include <memory>

G_BEGIN_DECLS

#define GVA_HISTORY_NAME "[Preview] Generic History Element"
#define GVA_HISTORY_DESCRIPTION "Performs caching of processing results"

GST_DEBUG_CATEGORY_EXTERN(gva_history_debug_category);
#define GST_DEBUG_CAT_GVA_HISTORY gva_history_debug_category

#define GST_TYPE_GVA_HISTORY (gva_history_get_type())
#define GVA_HISTORY(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_HISTORY, GvaHistory))
#define GVA_HISTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_HISTORY, GvaHistoryClass))
#define GST_IS_GVA_HISTORY(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_HISTORY))
#define GST_IS_GVA_HISTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_HISTORY))

enum HistoryType { META_HISTORY };

typedef struct _GvaHistory {
    GstBaseTransform base;

    struct _Props {
        /* public properties */
        HistoryType type;
        guint interval;

        /* private properties */
        std::unique_ptr<IHistory> processor;
    } props;
} GvaHistory;

typedef struct _GvaHistoryClass {
    GstBaseTransformClass base_class;
} GvaHistoryClass;

GType gva_history_get_type(void);

G_END_DECLS
