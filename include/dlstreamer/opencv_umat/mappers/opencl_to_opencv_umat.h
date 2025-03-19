/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/memory_mapper.h"
#include "dlstreamer/opencl/tensor.h"
#include "dlstreamer/opencv_umat/tensor.h"
#include "dlstreamer/opencv_umat/utils.h"

namespace dlstreamer {

class MemoryMapperOpenCLToOpenCVUMat : public BaseMemoryMapper {
  public:
    using BaseMemoryMapper::BaseMemoryMapper;

    TensorPtr map(TensorPtr src, AccessMode /*mode*/) override {
        cl_mem mem = ptr_cast<OpenCLTensor>(src)->clmem();
        auto &info = src->info();
        ImageInfo iminfo(info);

        cv::UMat umat;
        int dtype = CV_MAKETYPE(data_type_to_opencv(info.dtype), iminfo.channels());
        cv::ocl::convertFromBuffer(mem, iminfo.width_stride(), iminfo.height(), iminfo.width(), dtype, umat);
        auto ret = std::make_shared<OpenCVUMatTensor>(umat, info);
        ret->set_parent(src);
        return ret;
    }
};

} // namespace dlstreamer
