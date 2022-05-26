/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/context.h"

#ifdef DLS_HAVE_OPENCL
#include <CL/cl.h>
#else
struct _cl_context;
using cl_context = _cl_context *;
#endif

namespace dlstreamer {

class OpenCLContext : public Context {
  public:
    static constexpr auto context_name = "OpenCLContext";
    static constexpr auto cl_context_id = "cl_context";

    OpenCLContext(cl_context ctx) {
        _ctx = ctx;
    }

    cl_context context() {
        return _ctx;
    }

    std::vector<std::string> keys() const override {
        return {cl_context_id};
    }

    void *handle(std::string const &handle_id) const override {
        if (handle_id == cl_context_id)
            return _ctx;
        return nullptr;
    }

  protected:
    cl_context _ctx;
};

#ifdef DLS_HAVE_OPENCL

class OpenCLContextRefCounted : public OpenCLContext {
    OpenCLContextRefCounted(cl_context ctx) : OpenCLContext(ctx) {
        clRetainContext(ctx);
    }
    ~OpenCLContextRefCounted() {
        if (_ctx) {
            clReleaseContext(_ctx);
        }
    }
};

#else // !DLS_HAVE_OPENCL

class OpenCLContextRefCounted : public OpenCLContext {
    OpenCLContextRefCounted(cl_context ctx) : OpenCLContext(ctx) {
        // STUB
        throw std::runtime_error("Couldn't create OpenCL context: project was built without OpenCL support");
    }
};

#endif

using OpenCLContextPtr = std::shared_ptr<OpenCLContext>;

} // namespace dlstreamer
