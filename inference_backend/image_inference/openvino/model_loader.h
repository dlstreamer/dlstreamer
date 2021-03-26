/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once

#include <inference_engine.hpp>

namespace InferenceBackend {

struct NetworkReferenceWrapper {
  public:
    NetworkReferenceWrapper(const InferenceEngine::CNNNetwork &cnn_network,
                            const InferenceEngine::ExecutableNetwork &exe_network)
        : cnn_network_ptr(&cnn_network), exe_network_ptr(&exe_network) {
    }
    NetworkReferenceWrapper(const NetworkReferenceWrapper &) = default;

    const InferenceEngine::CNNNetwork &getCNN() const {
        return *cnn_network_ptr;
    }
    const InferenceEngine::ExecutableNetwork &getExecutable() const {
        return *exe_network_ptr;
    }

  private:
    const InferenceEngine::CNNNetwork *cnn_network_ptr;
    const InferenceEngine::ExecutableNetwork *exe_network_ptr;
};

struct ModelLoader {
    virtual ~ModelLoader() = default;

    virtual InferenceEngine::CNNNetwork load(InferenceEngine::Core &core, const std::string &model_xml,
                                             const std::map<std::string, std::string> &base_config) = 0;

    virtual std::string name(const NetworkReferenceWrapper &network) = 0;

    virtual InferenceEngine::ExecutableNetwork import(InferenceEngine::CNNNetwork &network, const std::string &model,
                                                      InferenceEngine::Core &core,
                                                      const std::map<std::string, std::string> &base_config,
                                                      const std::map<std::string, std::string> &inference_config) = 0;

    static bool is_compile_model(const std::string &model_path);
    static bool is_valid_model_path(const std::string &model_path);
};

struct IrModelLoader : ModelLoader {
    void *_display;
    IrModelLoader(void *display = nullptr) : _display(display){};
    InferenceEngine::CNNNetwork load(InferenceEngine::Core &core, const std::string &model_xml,
                                     const std::map<std::string, std::string> &base_config) override;

    std::string name(const NetworkReferenceWrapper &network) override;

    InferenceEngine::ExecutableNetwork import(InferenceEngine::CNNNetwork &network, const std::string &,
                                              InferenceEngine::Core &core,
                                              const std::map<std::string, std::string> &base_config,
                                              const std::map<std::string, std::string> &inference_config) override;
};

struct CompiledModelLoader : ModelLoader {

    InferenceEngine::CNNNetwork load(InferenceEngine::Core &core, const std::string &model_xml,
                                     const std::map<std::string, std::string> &base_config) override;

    std::string name(const NetworkReferenceWrapper &network) override;

    InferenceEngine::ExecutableNetwork import(InferenceEngine::CNNNetwork &, const std::string &model,
                                              InferenceEngine::Core &core,
                                              const std::map<std::string, std::string> &base_config,
                                              const std::map<std::string, std::string> &inference_config) override;
};

} // namespace InferenceBackend
