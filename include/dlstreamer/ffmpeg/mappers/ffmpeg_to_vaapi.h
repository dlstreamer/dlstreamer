/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/memory_mapper.h"
#include "dlstreamer/ffmpeg/context.h"
#include "dlstreamer/ffmpeg/frame.h"
#include "dlstreamer/utils.h"
#include "dlstreamer/vaapi/frame.h"
#include "dlstreamer/vaapi/utils.h"

namespace dlstreamer {

class MemoryMapperFFMPEGToVAAPI : public BaseMemoryMapper {
  public:
    MemoryMapperFFMPEGToVAAPI(const ContextPtr &input_context, const ContextPtr &output_context)
        : BaseMemoryMapper(input_context, output_context) {
    }

    TensorPtr map(TensorPtr tensor, AccessMode /*mode*/) override {
        return ptr_cast<VAAPITensor>(tensor);
    }
};

} // namespace dlstreamer
