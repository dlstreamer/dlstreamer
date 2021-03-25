/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
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
#include "input_image_layer_descriptor.h"

namespace InferenceBackend {

class OutputBlob;
class Allocator;
class InputLayerDesc;
class InputImageLayerDesc;
class ImageTransformationParams;

class ImageInference {
  public:
    using Ptr = std::shared_ptr<ImageInference>;

    // Application can derive and put object instance into inference queue, see last parameter in Submit* functions

    struct IFrameBase {
      protected:
        ImageTransformationParams::Ptr image_trans_params = std::make_shared<ImageTransformationParams>();

      public:
        using Ptr = std::shared_ptr<IFrameBase>;
        virtual void SetImage(const std::shared_ptr<Image> &image) = 0;
        virtual ImageTransformationParams::Ptr GetImageTransformationParams() {
            return image_trans_params;
        }

        virtual ~IFrameBase() = default;
    };

    typedef std::function<void(std::map<std::string, std::shared_ptr<OutputBlob>> blobs,
                               std::vector<IFrameBase::Ptr> frames)>
        CallbackFunc;
    typedef std::function<void(std::vector<IFrameBase::Ptr> frames)> ErrorHandlingFunc;

    static Ptr make_shared(MemoryType type, const std::string &model,
                           const std::map<std::string, std::map<std::string, std::string>> &config,
                           Allocator *allocator, CallbackFunc callback, ErrorHandlingFunc error_handler,
                           const std::string &device_name);

    virtual void Init() = 0;

    virtual void SubmitImage(const Image &image, IFrameBase::Ptr user_data,
                             const std::map<std::string, std::shared_ptr<InputLayerDesc>> &input_preprocessors) = 0;

    virtual const std::string &GetModelName() const = 0;
    virtual void GetModelImageInputInfo(size_t &width, size_t &height, size_t &batch_size, int &format,
                                        int &memory_type) const = 0;

    virtual bool IsQueueFull() = 0;
    virtual void Flush() = 0;
    virtual void Close() = 0;

    virtual ~ImageInference() = default;
};

class Blob {
  public:
    enum class Layout { ANY = 0, NCHW = 1, NHWC = 2, NC = 193 };
    enum class Precision { FP32 = 10, U8 = 40 };
    virtual ~Blob() = default;
    virtual const std::vector<size_t> &GetDims() const = 0;
    virtual Layout GetLayout() const = 0;
    virtual Precision GetPrecision() const = 0;
};

class OutputBlob : public virtual Blob {
  public:
    using Ptr = std::shared_ptr<OutputBlob>;
    virtual const void *GetData() const = 0;
    virtual ~OutputBlob() = default;
};

class InputBlob : public virtual Blob {
  public:
    using Ptr = std::shared_ptr<InputBlob>;
    virtual void *GetData() = 0;
    virtual size_t GetIndexInBatch() const = 0;
    virtual ~InputBlob() = default;
};

struct InputLayerDesc {
    using Ptr = std::shared_ptr<InputLayerDesc>;
    std::string name;
    std::function<void(const InputBlob::Ptr &)> preprocessor;
    InputImageLayerDesc::Ptr input_image_preroc_params = nullptr;
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
__DECLARE_CONFIG_KEY(LAYER_PRECISION);
__DECLARE_CONFIG_KEY(FORMAT);
__DECLARE_CONFIG_KEY(DEVICE);
__DECLARE_CONFIG_KEY(NIREQ);
__DECLARE_CONFIG_KEY(CPU_EXTENSION);          // library with implementation of custom layers
__DECLARE_CONFIG_KEY(GPU_EXTENSION);          // path xml configuration file
__DECLARE_CONFIG_KEY(VPU_EXTENSION);          // path xml configuration file
__DECLARE_CONFIG_KEY(CPU_THROUGHPUT_STREAMS); // number inference requests running in parallel
__DECLARE_CONFIG_KEY(GPU_THROUGHPUT_STREAMS);
__DECLARE_CONFIG_KEY(VPU_DEVICE_ID);
__DECLARE_CONFIG_KEY(PRE_PROCESSOR_TYPE);
__DECLARE_CONFIG_KEY(IMAGE_FORMAT);
__DECLARE_CONFIG_KEY(RESHAPE);
__DECLARE_CONFIG_KEY(BATCH_SIZE);
__DECLARE_CONFIG_KEY(RESHAPE_WIDTH);
__DECLARE_CONFIG_KEY(RESHAPE_HEIGHT);
__DECLARE_CONFIG_KEY(image);
__DECLARE_CONFIG_KEY(CAPS_FEATURE);
#undef __DECLARE_CONFIG_KEY
#undef __CONFIG_KEY

} // namespace InferenceBackend
