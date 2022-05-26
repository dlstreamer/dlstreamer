/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
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
#include <tensor_layer_desc.hpp>

class TensorInference {
  public:
    using BlobMap = std::map<std::string, InferenceEngine::Blob::Ptr>;
    using CompletionCallback = std::function<void(const std::string &)>;

    struct PreProcInfo {
        InferenceEngine::ResizeAlgorithm resize_alg = InferenceEngine::ResizeAlgorithm::NO_RESIZE;
        InferenceEngine::ColorFormat color_format = InferenceEngine::ColorFormat::RAW;
        void *va_display = nullptr;
    };

    struct ImageInfo {
        int channels = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        InferenceBackend::MemoryType memory_type = InferenceBackend::MemoryType::SYSTEM;

        explicit operator bool() const {
            return channels != 0 && width != 0 && height != 0;
        }
    };

    struct RoiRect {
        RoiRect() = default;
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t w = 0;
        uint32_t h = 0;
        operator bool() const {
            return w != 0 && h != 0;
        }
    };

    TensorInference(const std::string &model_path);
    ~TensorInference();

    void Init(const std::string &device, size_t num_requests, const std::string &ie_config, const PreProcInfo &preproc,
              const ImageInfo &image);

    bool IsInitialized() const;

    const std::vector<TensorLayerDesc> &GetTensorInputInfo() const;
    const std::vector<TensorLayerDesc> &GetTensorOutputInfo() const;
    const std::vector<size_t> &GetTensorOutputSizes() const;
    std::string GetModelName() const;
    size_t GetRequestsNum() const;

    void InferAsync(const std::shared_ptr<FrameData> input, std::shared_ptr<FrameData> output,
                    CompletionCallback completion_callback, const RoiRect &roi);

    bool IsRunning() const;
    void Flush();

    void lock() {
        _object_lock.lock();
    }
    void unlock() {
        _object_lock.unlock();
    }

  private:
    struct Request {
        using Ptr = std::shared_ptr<Request>;

        InferenceEngine::InferRequest::Ptr infer_req;
        CompletionCallback completion_callback;
    };

    Request::Ptr PrepareRequest(const std::shared_ptr<FrameData> input, std::shared_ptr<FrameData> output,
                                const RoiRect &roi);
    void SetInputBlob(Request::Ptr request, const std::shared_ptr<FrameData> frame_data, const RoiRect &roi);
    void SetOutputBlob(Request::Ptr request, std::shared_ptr<FrameData> frame_data);
    InferenceEngine::Blob::Ptr MakeBlob(const InferenceEngine::TensorDesc &tensor_desc, uint8_t *data) const;
    InferenceEngine::Blob::Ptr MakeNV12VaapiBlob(const std::shared_ptr<FrameData> &frame_data) const;
    InferenceEngine::Blob::Ptr MakeNV12Blob(const std::shared_ptr<FrameData> &frame_data,
                                            InferenceEngine::TensorDesc tensor_desc, const RoiRect &roi) const;
    InferenceEngine::Blob::Ptr MakeI420Blob(const std::shared_ptr<FrameData> &frame_data,
                                            InferenceEngine::TensorDesc tensor_desc, const RoiRect &roi) const;
    InferenceEngine::Blob::Ptr MakeBGRBlob(const std::shared_ptr<FrameData> &frame_data,
                                           InferenceEngine::TensorDesc tensor_desc, const RoiRect &roi) const;

    void ConfigurePreProcessing(const PreProcInfo &preproc, const ImageInfo &image);

    void onInferCompleted(Request::Ptr request, InferenceEngine::StatusCode code);

  private:
    InferenceEngine::Core _ie;
    InferenceEngine::CNNNetwork _network;
    InferenceEngine::ExecutableNetwork _executable_net;
    bool _is_initialized = false;
    bool _ie_preproc_enabled = false;
    bool _vaapi_surface_sharing_enabled = false;
    void *_va_display = nullptr;
    size_t _num_requests = 0;

    TensorCaps _input_tensor_caps;

    PreProcInfo _pre_proc_info;
    ImageInfo _image_info;

    static std::map<std::string, std::string> base_config;
    static std::map<std::string, std::string> inference_config;

    std::vector<TensorLayerDesc> _input_info;
    std::vector<TensorLayerDesc> _output_info;
    std::vector<size_t> _output_sizes;

    mutable std::mutex _init_mutex;
    std::mutex _flush_mutex;
    std::condition_variable _request_processed;
    // Needed to infer all ROIs at once in one channel
    // TODO: remove when scheduler is implemented
    std::mutex _object_lock;

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

        size_t size() const {
            return _queue.size();
        }

      private:
        std::mutex _mutex;
        std::condition_variable _cond_variable;
        std::queue<Request::Ptr> _queue;

    } _free_requests;
};
