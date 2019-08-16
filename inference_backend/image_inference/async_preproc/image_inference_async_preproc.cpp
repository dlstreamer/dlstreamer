/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "image_inference_async_preproc.h"

namespace InferenceBackend {

ImageInferenceAsyncPreProc::ImageInferenceAsyncPreProc(Ptr inference, std::shared_ptr<PreProc> pre_proc,
                                                       int image_pool_size)
    : inference(inference), pre_proc(pre_proc), image_pool_size(image_pool_size) {
    images.resize(image_pool_size);

    int width = 0;
    int height = 0;
    int format = 0;
    inference->GetModelInputInfo(&width, &height, &format);

    for (PreprocImage &img : images) {
        img.image.type = MemoryType::ANY;
        img.image.width = width;
        img.image.height = height;
        img.image.format = format;
        img.completed = true;
        img.image_map = ImageMap::Create();
    }
}

ImageInferenceAsyncPreProc::~ImageInferenceAsyncPreProc() {
    for (PreprocImage &img : images) {
        pre_proc->ReleaseImage(img.image);
        if (img.image_map) {
            delete img.image_map;
        }
    }
}

ImageInferenceAsyncPreProc::PreprocImage *ImageInferenceAsyncPreProc::FindFreeImage() {
    std::unique_lock<std::mutex> lock(mutex_images);
    for (;;) {
        for (PreprocImage &img : images) {
            if (img.completed) {
                img.completed = false;
                return &img;
            }
        }
        condvar_images.wait(lock);
    }
}

void ImageInferenceAsyncPreProc::SubmitInference(PreprocImage *pp_image, IFramePtr user_data,
                                                 std::function<void(Image &)> preProcessor) {
    std::unique_lock<std::mutex> lock(mutex_inference);
    Image image_sys = pp_image->image_map->Map(pp_image->image);
    inference->SubmitImage(image_sys, user_data, preProcessor);
    pp_image->image_map->Unmap();
    pp_image->completed = true;
    condvar_images.notify_one();
}

void ImageInferenceAsyncPreProc::SubmitImage(const Image &image, IFramePtr user_data,
                                             std::function<void(Image &)> preProcessor) {
    PreprocImage *pp_image = FindFreeImage();
    pre_proc->Convert(image, pp_image->image, true);
    // create async task
    pp_image->sync = std::async(&ImageInferenceAsyncPreProc::SubmitInference, this, pp_image, user_data, preProcessor);
}

const std::string &ImageInferenceAsyncPreProc::GetModelName() const {
    return inference->GetModelName();
}

void ImageInferenceAsyncPreProc::GetModelInputInfo(int *width, int *height, int *format) const {
    inference->GetModelInputInfo(width, height, format);
}

bool ImageInferenceAsyncPreProc::IsQueueFull() {
    return inference->IsQueueFull();
}

void ImageInferenceAsyncPreProc::Flush() {
    {
        std::unique_lock<std::mutex> lock(mutex_images);
        for (PreprocImage &img : images) {
            if (!img.completed)
                img.sync.wait();
        }
    }
    inference->Flush();
}

void ImageInferenceAsyncPreProc::Close() {
    inference->Close();
}

} // namespace InferenceBackend
