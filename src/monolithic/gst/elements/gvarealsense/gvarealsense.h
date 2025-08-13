/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
/*
 * This header file defines the GStreamer element for Intel RealSense camera integration.
 *
 * - Includes necessary GStreamer and RealSense SDK headers.
 * - Declares unique pointer types for RealSense pipeline and config objects.
 * - Defines GObject type macros for the GstRealSense element.
 * - Declares the GstRealSense and GstRealSenseClass structures:
 *      - GstRealSense extends GstBaseSrc and holds:
 *          - URI string for camera source.
 *          - RealSense pipeline and configuration objects.
 *          - GStreamer video format and caps.
 * - Declares the function to get the GType for the element.
 */

#ifndef __GST_REAL_SENSE_H__
#define __GST_REAL_SENSE_H__

#include <gst/base/gstbasesrc.h>
#include <gst/gst.h>
#include <gst/gstcaps.h>
#include <gst/video/video-format.h>
#include <gst/video/video-info.h>
#include <gst/video/video.h>

// Real Sense
#include <librealsense2/rs.hpp>

G_BEGIN_DECLS

#define GST_TYPE_GVAREALSENSE (gst_real_sense_get_type())
// G_DECLARE_FINAL_TYPE(GstGvaRealSense, gst_gvarealsensedeskew, GST, GVAREALSENSE, GstVideoFilter)

GST_DEBUG_CATEGORY_STATIC(gst_real_sense_debug);
#define GST_CAT_DEFAULT gst_real_sense_debug

// Define a unique pointer type for rs2::pipeline
using rsPipelinePtr = std::unique_ptr<rs2::pipeline>;

// Define a unique pointer type for rs2::config
using rsConfigPtr = std::unique_ptr<rs2::config>;

#define GST_TYPE_REAL_SENSE (gst_real_sense_get_type())
#define GST_REAL_SENSE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_REAL_SENSE, GstRealSense))
#define GST_REAL_SENSE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_REAL_SENSE, GstRealSenseClass))
#define GST_IS_REAL_SENSE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_REAL_SENSE))
#define GST_IS_REAL_SENSE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_REAL_SENSE))
#define GST_REAL_SENSE_CAST(obj) ((GstRealSense *)obj)

typedef struct _GstRealSense GstRealSense;
typedef struct _GstRealSenseClass GstRealSenseClass;

#define struct_stat struct stat

/**
 * GstRealSense:
 *
 * Opaque #GstRealSense structure.
 */
struct _GstRealSense {
    GstBaseSrc element;

    /*< private >*/
    gchar *uri;

    /* Private for Real Sense camera: */
    rsPipelinePtr rsPipeline = nullptr; // Real Sense pipeline
    rs2::config rsCfg;                  // Real Sense configuration
    GstVideoFormat gstVideoFormat;
    GstCaps *gstCaps = nullptr;

    uint64_t frameCount = 0; // Frame counter for debugging
};

struct _GstRealSenseClass {
    GstBaseSrcClass parent_class;
};

G_GNUC_INTERNAL GType gst_real_sense_get_type(void);

G_END_DECLS

#endif /* __GST_REAL_SENSE_H__ */
