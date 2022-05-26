
/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <inference_backend/input_image_layer_descriptor.h>

#include <gst/gst.h>
#include <gst/video/video.h>

#pragma once

class IPreProcElem {
  public:
    virtual bool start() {
        return true;
    };
    virtual bool stop() {
        return true;
    };
    virtual bool set_property(guint /*prop_id*/, const GValue * /*value*/) {
        return false;
    }
    virtual bool get_property(guint /*prop_id*/, GValue * /*value*/) {
        return false;
    }
    virtual bool need_preprocessing() const {
        return true;
    }
    virtual void flush(){};
    virtual void init_preprocessing(const InferenceBackend::InputImageLayerDesc::Ptr &pre_proc_info,
                                    GstCaps *input_caps, GstCaps *output_caps) = 0;

    virtual GstFlowReturn run_preproc(GstBuffer *inbuf, GstBuffer *outbuf = nullptr,
                                      GstVideoRegionOfInterestMeta *roi = nullptr) const = 0;
    virtual gboolean transform_size(GstPadDirection direction, GstCaps *caps, gsize size, GstCaps *othercaps,
                                    gsize *othersize) = 0;
};
