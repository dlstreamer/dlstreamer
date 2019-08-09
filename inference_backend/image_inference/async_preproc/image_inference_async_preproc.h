/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"
#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "inference_backend/pre_proc.h"

#include <future>
#include <iostream>
#include <map>
#include <string>
#include <thread>

namespace InferenceBackend {

class ImageInferenceAsyncPreProc : public ImageInference {
  public:
    ImageInferenceAsyncPreProc(Ptr inference, std::shared_ptr<PreProc> pre_proc, int image_pool_size);

    ~ImageInferenceAsyncPreProc();

    virtual void SubmitImage(const Image &image, IFramePtr user_data, std::function<void(Image &)> preProcessor);

    virtual const std::string &GetModelName() const;

    virtual void GetModelInputInfo(int *width, int *height, int *format) const;

    virtual bool IsQueueFull();

    virtual void Flush();

    virtual void Close();

  private:
    struct PreprocImage {
        Image image;
        std::future<void> sync;
        volatile bool completed;
        ImageMap *image_map;
    };

    Ptr inference;
    std::shared_ptr<PreProc> pre_proc;
    int image_pool_size;

    std::vector<PreprocImage> images;

    // mutex for images vector
    mutable std::mutex mutex_images;
    std::condition_variable condvar_images;

    // mutex for inference
    mutable std::mutex mutex_inference;

    PreprocImage *FindFreeImage();
    void SubmitInference(PreprocImage *pp_image, IFramePtr user_data, std::function<void(Image &)> preProcessor);
};

} // namespace InferenceBackend
