/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
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

    static Ptr make_shared(MemoryType type, std::string devices, std::string model, int batch_size, int nireq,
                           const std::map<std::string, std::string> &config, CallbackFunc callback);

    virtual void SubmitImage(const Image &image, IFramePtr user_data, std::function<void(Image &)> preProcessor) = 0;

    virtual const std::string &GetModelName() const = 0;
    virtual const std::string &GetLayerTypeByLayerName(const std::string &layer_name) const = 0;

    virtual bool IsQueueFull() = 0;
    virtual void Flush() = 0;
    virtual void Close() = 0;

    virtual ~ImageInference() = default;
};

class OutputBlob {
  public:
    using Ptr = std::shared_ptr<OutputBlob>;
    enum class Layout { ANY = 0, NCHW = 1, NHWC = 2 };
    enum class Precision { FP32 = 10, U8 = 40 };
    virtual ~OutputBlob() = default;
    virtual const std::vector<size_t> &GetDims() const = 0;
    virtual Layout GetLayout() const = 0;
    virtual Precision GetPrecision() const = 0;
    virtual const void *GetData() const = 0;
};

#define __CONFIG_KEY(name) KEY_##name
#define __DECLARE_CONFIG_KEY(name) static constexpr auto __CONFIG_KEY(name) = #name
__DECLARE_CONFIG_KEY(CPU_EXTENSION);          // library with implementation of custom layers
__DECLARE_CONFIG_KEY(CPU_THROUGHPUT_STREAMS); // number inference requests running in parallel
__DECLARE_CONFIG_KEY(RESIZE_BY_INFERENCE);    // experimental, don't use

} // namespace InferenceBackend
