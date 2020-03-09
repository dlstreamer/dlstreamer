/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "image.h"

namespace InferenceBackend {

class OutputBlob;
class Allocator;

class ImageInference {
  public:
    using Ptr = std::shared_ptr<ImageInference>;

    // Application can derive and put object instance into inference queue, see last parameter in Submit* functions
    struct IFrameBase {
        virtual ~IFrameBase() = default;
    };

    typedef std::shared_ptr<IFrameBase> IFramePtr;
    typedef std::function<void(std::map<std::string, std::shared_ptr<OutputBlob>> blobs, std::vector<IFramePtr> frames)>
        CallbackFunc;

    static Ptr make_shared(MemoryType type, const std::string &model,
                           const std::map<std::string, std::map<std::string, std::string>> &config,
                           Allocator *allocator, CallbackFunc callback);

    virtual void SubmitImage(const Image &image, IFramePtr user_data, std::function<void(Image &)> preProcessor) = 0;

    virtual const std::string &GetModelName() const = 0;
    virtual void GetModelInputInfo(int *width, int *height, int *batch_size, int *format) const = 0;

    virtual bool IsQueueFull() = 0;
    virtual void Flush() = 0;
    virtual void Close() = 0;

    virtual ~ImageInference() = default;
};

class OutputBlob {
  public:
    using Ptr = std::shared_ptr<OutputBlob>;
    enum class Layout { ANY = 0, NCHW = 1, NHWC = 2, NC = 193 };
    enum class Precision { FP32 = 10, U8 = 40 };
    virtual ~OutputBlob() = default;
    virtual const std::vector<size_t> &GetDims() const = 0;
    virtual size_t GetSize() const = 0;
    virtual Layout GetLayout() const = 0;
    virtual Precision GetPrecision() const = 0;
    virtual const void *GetData() const = 0;
};

class Allocator {
  public:
    struct AllocContext;
    virtual void Alloc(size_t size, void *&buffer_ptr, AllocContext *&alloc_context) = 0;
    virtual void Free(AllocContext *alloc_context) = 0;
};

#define __CONFIG_KEY(name) KEY_##name
#define __DECLARE_CONFIG_KEY(name) static constexpr auto __CONFIG_KEY(name) = #name
__DECLARE_CONFIG_KEY(BASE);
__DECLARE_CONFIG_KEY(INFERENCE);
__DECLARE_CONFIG_KEY(DEVICE);
__DECLARE_CONFIG_KEY(NIREQ);
__DECLARE_CONFIG_KEY(CPU_EXTENSION);          // library with implementation of custom layers
__DECLARE_CONFIG_KEY(GPU_EXTENSION);          // path xml configuration file
__DECLARE_CONFIG_KEY(VPU_EXTENSION);          // path xml configuration file
__DECLARE_CONFIG_KEY(CPU_THROUGHPUT_STREAMS); // number inference requests running in parallel
__DECLARE_CONFIG_KEY(GPU_THROUGHPUT_STREAMS);
__DECLARE_CONFIG_KEY(PRE_PROCESSOR_TYPE);
__DECLARE_CONFIG_KEY(IMAGE_FORMAT);
__DECLARE_CONFIG_KEY(RESHAPE);
__DECLARE_CONFIG_KEY(BATCH_SIZE);
__DECLARE_CONFIG_KEY(RESHAPE_WIDTH);
__DECLARE_CONFIG_KEY(RESHAPE_HEIGHT);

} // namespace InferenceBackend
