/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once
#include "inference_backend/image_inference.h"
#include "inference_backend/pre_proc.h"
#include "model_loader.h"

#include <inference_engine.hpp>

struct EntityBuilder {
    EntityBuilder() = delete;
    EntityBuilder(const EntityBuilder &) = delete;

    virtual ~EntityBuilder() = default;

    virtual InferenceEngine::CNNNetwork createNetwork() {
        return loader->load(model_path, base_config);
    };

    virtual std::tuple<std::unique_ptr<InferenceBackend::ImagePreprocessor>, InferenceEngine::ExecutableNetwork,
                       std::string>
    createPreProcAndExecutableNetwork(InferenceEngine::CNNNetwork &network) {
        return createPreProcAndExecutableNetwork_impl(network);
    }

    virtual std::string getNetworkName(const InferenceBackend::NetworkReferenceWrapper &network) {
        return loader->name(network);
    }

  private:
    virtual std::tuple<std::unique_ptr<InferenceBackend::ImagePreprocessor>, InferenceEngine::ExecutableNetwork,
                       std::string>
    createPreProcAndExecutableNetwork_impl(InferenceEngine::CNNNetwork &network) = 0;

  protected:
    EntityBuilder(std::unique_ptr<InferenceBackend::ModelLoader> &&loader,
                  const InferenceBackend::InferenceConfig &config, const std::string &model_path)
        : loader(std::move(loader)), base_config(config.at(InferenceBackend::KEY_BASE)),
          inference_config(config.at(InferenceBackend::KEY_INFERENCE)),
          input_layer_precision_config(config.at(InferenceBackend::KEY_INPUT_LAYER_PRECISION)),
          layer_format_config(config.at(InferenceBackend::KEY_FORMAT)),
          batch_size(safe_convert<size_t>(
              std::stoi(config.at(InferenceBackend::KEY_BASE).at(InferenceBackend::KEY_BATCH_SIZE)))),
          model_path(model_path) {
    }

    std::unique_ptr<InferenceBackend::ModelLoader> loader;
    const std::map<std::string, std::string> base_config;
    std::map<std::string, std::string> inference_config;
    const std::map<std::string, std::string> input_layer_precision_config;
    const std::map<std::string, std::string> layer_format_config;
    const size_t batch_size;
    const std::string model_path;
};

struct IrBuilder : EntityBuilder {
    IrBuilder(const InferenceBackend::InferenceConfig &config, const std::string &model_path,
              InferenceEngine::RemoteContext::Ptr remote_ctx = nullptr)
        : EntityBuilder(std::unique_ptr<InferenceBackend::ModelLoader>(new InferenceBackend::IrModelLoader(remote_ctx)),
                        config, model_path) {
    }

  private:
    void checkLayersConfig(const InferenceEngine::InputsDataMap &inputs_info);
    void configureNetworkLayers(const InferenceEngine::InputsDataMap &inputs_info, std::string &image_input_name);
    std::tuple<std::unique_ptr<InferenceBackend::ImagePreprocessor>, InferenceEngine::ExecutableNetwork, std::string>
    createPreProcAndExecutableNetwork_impl(InferenceEngine::CNNNetwork &network) override;
};

struct CompiledBuilder : EntityBuilder {
    CompiledBuilder(const InferenceBackend::InferenceConfig &config, const std::string &model_path,
                    InferenceEngine::RemoteContext::Ptr remote_ctx = nullptr)
        : EntityBuilder(
              std::unique_ptr<InferenceBackend::ModelLoader>(new InferenceBackend::CompiledModelLoader(remote_ctx)),
              config, model_path) {
    }

  private:
    std::tuple<std::unique_ptr<InferenceBackend::ImagePreprocessor>, InferenceEngine::ExecutableNetwork, std::string>
    createPreProcAndExecutableNetwork_impl(InferenceEngine::CNNNetwork &network);
};
