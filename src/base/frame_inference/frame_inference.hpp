/*******************************************************************************
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <list>
#include <mutex>
#include <spdlog/spdlog.h>

#include "dlstreamer/base/frame.h"
#include "dlstreamer/context.h"
#include "dlstreamer/frame_info.h"

#include "input_model_preproc.h"

namespace dlstreamer {

// TODO: Is it good?
enum class PreprocessBackend {
    Auto = 0,
    OpenVino = 1,
    VaApi = 2,
    VaApiSurfaceSharing = 3,
    OpenCv = 4,
};

// TODO: replace with Dictionary ?
struct FrameInferenceParams {
    std::string model_path;
    std::string device;

    std::string ov_config_str;
    std::map<std::string, std::string> ov_config_map;

    uint32_t batch_size = 0;
    uint32_t nireq = 0;
    PreprocessBackend preprocess_be;

    std::string logger_name;

    // FIXME: depends on GST
    // FIXME: use DictionaryCPtr ?
    std::vector<ModelInputProcessorInfo::Ptr> preprocessing_params;
    std::map<std::string, DictionaryPtr> postprocessing_params;

    static std::optional<PreprocessBackend> preprocess_backend_from_string(const std::string &ppb_name);
};

class FrameInference final {
  public:
    using FrameReadyCallback = std::function<void(FramePtr)>;

    FrameInference(const FrameInferenceParams &params, ContextPtr app_context, MemoryType in_memory_type,
                   FrameInfo &_input_info);

    ~FrameInference();

    // NO COPY
    FrameInference(const FrameInference &) = delete;
    FrameInference &operator=(const FrameInference &) = delete;

    void run_async(FramePtr frame, FrameReadyCallback ready_cb);

    void flush();

  private:
    std::unique_ptr<class OpenVinoBackend> _ov_backend;

    ContextPtr _app_context;
    MemoryMapperPtr _input_mapper;

    std::unique_ptr<class TransformInplace> _post_processing_elem;

    struct ListEntry {
        ListEntry(FramePtr f, FrameReadyCallback cb) : frame(std::move(f)), ready_callback(std::move(cb)) {
        }
        ListEntry() = delete;
        ListEntry(const ListEntry &) = delete;
        ListEntry &operator=(const ListEntry &) = delete;
        ListEntry(ListEntry &&) = default;
        ListEntry &operator=(ListEntry &&) = default;
        FramePtr frame;
        FrameReadyCallback ready_callback;
    };

    std::list<ListEntry> _frames_list;
    std::mutex _frames_list_mutex;

    std::shared_ptr<spdlog::logger> _log;
    MemoryType _input_memory_type;
    FrameInfo _input_info;

    struct Stub {
        std::list<FramePtr> infer_list;
        std::mutex infer_list_mutex;

        std::thread ov_completion_thread;
        std::condition_variable ov_signal;
        bool thread_running = true;
        bool flush = false;
    } _stub;

  private:
    void init_inference_backend(const FrameInferenceParams &params);
    void init_post_processing(const FrameInferenceParams &params);

    void start_inference_internal(FramePtr frame);

    void on_inference_complete(FramePtr frame, TensorVector output_tensors);
    void postprocess(FramePtr frame, TensorVector output_tensors);

    void fake_start_inference_internal(FramePtr frame);
    void fake_ov_worker();
};

} // namespace dlstreamer