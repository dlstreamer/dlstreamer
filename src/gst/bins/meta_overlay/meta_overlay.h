/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "processbin.h"
#include <memory>

#include <gst/gst.h>

G_BEGIN_DECLS

#define META_OVERLAY_BIN_NAME                                                                                          \
    "Bin element for detection/classification/recognition results displaying/overlaying/drawing"
#define META_OVERLAY_BIN_DESCRIPTION "Overlays the metadata on the video frame to visualize the inference results."

#define GST_TYPE_META_OVERLAY_BIN (meta_overlay_bin_get_type())
#define GST_META_OVERLAY_BIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_META_OVERLAY_BIN, GstMetaOverlayBin))
#define GST_META_OVERLAY_BIN_CLASS(klass)                                                                              \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_META_OVERLAY_BIN, GstMetaOverlayBinClass))
#define GST_IS_META_OVERLAY_BIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_META_OVERLAY_BIN))
#define GST_IS_META_OVERLAY_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_META_OVERLAY_BIN))

typedef struct _GstMetaOverlayBin {
    GstProcessBin process_bin;

    std::shared_ptr<class MetaOverlayBinPrivate> impl;
} GstMetaOverlayBin;

typedef struct _GstMetaOverlayBinClass {
    GstProcessBinClass process_bin_class;
} GstMetaOverlayBinClass;

GType meta_overlay_bin_get_type(void);

G_END_DECLS
