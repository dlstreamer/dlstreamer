/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/gst/utils.h"
#include "dlstreamer/opencl/context.h"

namespace dlstreamer {

class GSTOpenCLContext : public OpenCLContext {
  public:
    GSTOpenCLContext(GstPad *pad) : OpenCLContext(nullptr) {
        query_context(pad);
    }
    GSTOpenCLContext(GstBaseTransform *element) : OpenCLContext(nullptr) {
        query_context(element->sinkpad);
    }
    ~GSTOpenCLContext() {
        if (_context) {
            gst_context_unref(_context);
            _context = nullptr;
        }
    }

  private:
    GstContext *_context = nullptr;

    static constexpr auto GST_OPENCL_CONTEXT_FIELD_NAME = "cl_context";

    void query_context(GstPad *pad) {
        _context = gst_query_context(pad, OpenCLContext::context_name);

        const GstStructure *structure = gst_context_get_structure(_context);
        auto ret = gst_structure_get(structure, GST_OPENCL_CONTEXT_FIELD_NAME, G_TYPE_POINTER, &_ctx, NULL);
        if (!ret || !_ctx)
            throw std::runtime_error("Couldn't get field from GST context");

        GST_INFO_OBJECT(pad, "Got cl_context from OpenCL context: %p", _ctx);
    }
};

} // namespace dlstreamer
