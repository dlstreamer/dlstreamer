/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_backend/pre_proc.h"

#include <opencv2/opencv.hpp>

namespace InferenceBackend {

class GAPI_VPP : public PreProc {
  public:
    GAPI_VPP();
    ~GAPI_VPP();

    // PreProc interface
    void Convert(const Image &src, Image &dst, bool bAllocateDestination);
    void ReleaseImage(const Image &dst);
};

} // namespace InferenceBackend
