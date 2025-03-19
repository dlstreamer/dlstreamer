/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/gst/frame.h"

#include <gst/gstbufferlist.h>

namespace dlstreamer {

class GSTFrameBatch : public GSTFrame {
  public:
    GSTFrameBatch(GstBufferList *buffer_list, const FrameInfo &info, bool take_ownership = false,
                  ContextPtr context = nullptr)
        : GSTFrame(info), _gst_buffer_list(buffer_list) {
        FrameInfo new_info = info;
        // first tensor only from each buffer
        new_info.tensors.resize(1);
        for (guint i = 0; i < gst_buffer_list_length(buffer_list); i++) {
            GstBuffer *buffer = gst_buffer_list_get(buffer_list, i);
            init(buffer, new_info, context);
        }
        _take_ownership = take_ownership;
        _metadata = std::make_unique<GSTMetadata>(buffer_list);
    }
    ~GSTFrameBatch() {
        if (_take_ownership && _gst_buffer_list)
            gst_buffer_list_unref(_gst_buffer_list);
    }

  protected:
    GstBufferList *_gst_buffer_list = nullptr;
};

} // namespace dlstreamer
