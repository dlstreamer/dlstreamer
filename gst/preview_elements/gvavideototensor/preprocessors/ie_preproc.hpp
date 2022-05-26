/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "ipreproc.hpp"

#include <ie_preprocess.hpp>

class IEPreProc : public IPreProc {
  public:
    IEPreProc(GstVideoInfo *video_info);

    void process(GstBuffer *in_buffer, GstBuffer *out_buffer = nullptr,
                 GstVideoRegionOfInterestMeta *roi = nullptr) final;

    const InferenceEngine::PreProcessInfo *info() const {
        return _pre_proc_info.get();
    }

    void flush() final{};

    size_t output_size() const final {
        return 0;
    }

    bool need_preprocessing() const final {
        return true;
    }

  private:
    GstVideoInfo *_video_info;
    std::unique_ptr<InferenceEngine::PreProcessInfo> _pre_proc_info;
};
