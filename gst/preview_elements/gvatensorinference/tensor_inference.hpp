/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include <ie_core.hpp>

#include <frame_data.hpp>
#include <memory_type.hpp>

class TensorInference {
  public:
    using BlobMap = std::map<std::string, InferenceEngine::Blob::Ptr>;
    using CompletionCallback = std::function<void(const std::string &)>;

    TensorInference(const std::string &model_path);

    // Simplified information about the input image of the model.
    struct TensorInputInfo {
        uint8_t precision = InferenceEngine::Precision::UNSPECIFIED;
        uint8_t layout = InferenceEngine::Layout::ANY;
        std::vector<size_t> dims;
        size_t width = 0;
        size_t height = 0;
        size_t channels = 0;

        explicit operator bool() const {
            return dims.size() > 0;
        }
    };

    struct TensorOutputInfo {
        uint8_t precision = InferenceEngine::Precision::UNSPECIFIED;
        uint8_t layout = InferenceEngine::Layout::ANY;
        std::vector<size_t> dims;
        size_t width = 0;
        size_t height = 0;
        size_t channels = 0;
        size_t size = 1;

        explicit operator bool() const {
            return dims.size() > 0;
        }
    };

    struct PreProcInfo {
        InferenceEngine::ResizeAlgorithm resize_alg = InferenceEngine::ResizeAlgorithm::NO_RESIZE;
        InferenceEngine::ColorFormat color_format = InferenceEngine::ColorFormat::RAW;
    };

    struct ImageInfo {
        int channels = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        void *va_display = nullptr;
        InferenceBackend::MemoryType memory_type = InferenceBackend::MemoryType::SYSTEM;

        explicit operator bool() const {
            return channels != 0 && width != 0 && height != 0;
        }
    };

    void Init(const std::string &device, size_t num_requests, const std::string &ie_config, const PreProcInfo &preproc,
              const ImageInfo &image);

    TensorInputInfo GetTensorInputInfo() const;
    TensorOutputInfo GetTensorOutputInfo() const;

    void InferAsync(const std::shared_ptr<FrameData> input, std::shared_ptr<FrameData> output,
                    CompletionCallback completion_callback);

  private:
    InferenceEngine::Core _ie;
    InferenceEngine::CNNNetwork _network;
    InferenceEngine::ExecutableNetwork _executable_net;
    bool _is_initialized = false;
    bool _ie_preproc_enabled = false;
    bool _vaapi_surface_sharing_enabled = false;
    void *_va_display = nullptr;

    TensorCaps _input_tensor_caps;

    PreProcInfo _pre_proc_info;
    ImageInfo _image_info;

    static std::map<std::string, std::string> base_config;
    static std::map<std::string, std::string> inference_config;

    mutable TensorInputInfo _input_info;
    mutable TensorOutputInfo _output_info;

    struct Request {
        using Ptr = std::shared_ptr<Request>;

        InferenceEngine::InferRequest::Ptr infer_req;
        CompletionCallback completion_callback;
    };

    struct RequestsPool {
        Request::Ptr pop() {
            std::unique_lock<std::mutex> lock(_mutex);
            while (_queue.empty())
                _cond_variable.wait(lock);

            auto item = _queue.front();
            _queue.pop();
            return item;
        }

        void push(Request::Ptr item) {
            _mutex.lock();
            _queue.push(item);
            _mutex.unlock();
            _cond_variable.notify_one();
        }

      private:
        std::mutex _mutex;
        std::condition_variable _cond_variable;
        std::queue<Request::Ptr> _queue;

    } _free_requests;

    Request::Ptr PrepareRequest(const std::shared_ptr<FrameData> input, std::shared_ptr<FrameData> output);
    void SetInputBlob(Request::Ptr request, const std::shared_ptr<FrameData> frame_data);
    void SetOutputBlob(Request::Ptr request, std::shared_ptr<FrameData> frame_data);
    InferenceEngine::Blob::Ptr MakeBlob(const InferenceEngine::TensorDesc &tensor_desc, uint8_t *data);
    InferenceEngine::Blob::Ptr MakeNV12VaapiBlob(const std::shared_ptr<FrameData> frame_data);

    void ConfigurePreProcessing(const PreProcInfo &preproc, const ImageInfo &image);

    void onInferCompleted(Request::Ptr request, InferenceEngine::StatusCode code);
};
