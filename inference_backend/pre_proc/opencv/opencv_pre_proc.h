/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_backend/pre_proc.h"

namespace InferenceBackend {

class OpenCV_VPP : public PreProc {
  public:
    OpenCV_VPP();
    ~OpenCV_VPP();

    // PreProc interface
    void Convert(const Image &src, Image &dst, bool bAllocateDestination);
    void ReleaseImage(const Image &);
};

} // namespace InferenceBackend
