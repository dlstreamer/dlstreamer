/*******************************************************************************
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

struct GvaInference {
    GstBaseTransform parent;

    struct GvaInferencePrivate *impl;
};

struct GvaInferenceClass {
    GstBaseTransformClass parent_class;
};

GType gva_inference_get_type();

#define GVA_INFERENCE_CAST(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), gva_inference_get_type(), GvaInference))

G_END_DECLS