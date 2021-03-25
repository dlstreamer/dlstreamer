/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
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

    virtual InferenceEngine::CNNNetwork createNetwork(InferenceEngine::Core &core) {
        return loader->load(core, model_path, base_config);
    };

    virtual std::tuple<std::unique_ptr<InferenceBackend::ImagePreprocessor>, InferenceEngine::ExecutableNetwork,
                       std::string>
    createPreProcAndExecutableNetwork(InferenceEngine::CNNNetwork &network, InferenceEngine::Core &core) {
        return createPreProcAndExecutableNetwork_impl(network, core);
    }

    virtual std::string getNetworkName(const InferenceBackend::NetworkReferenceWrapper &network) {
        return loader->name(network);
    }

  private:
    virtual std::tuple<std::unique_ptr<InferenceBackend::ImagePreprocessor>, InferenceEngine::ExecutableNetwork,
                       std::string>
    createPreProcAndExecutableNetwork_impl(InferenceEngine::CNNNetwork &network, InferenceEngine::Core &core) = 0;

  protected:
    EntityBuilder(std::unique_ptr<InferenceBackend::ModelLoader> &&loader,
                  const std::map<std::string, std::map<std::string, std::string>> &config,
                  const std::string &model_path)
        : loader(std::move(loader)), base_config(config.at(InferenceBackend::KEY_BASE)),
          inference_config(config.at(InferenceBackend::KEY_INFERENCE)),
          layer_precision_config(config.at(InferenceBackend::KEY_LAYER_PRECISION)),
          layer_type_config(config.at(InferenceBackend::KEY_FORMAT)),
          batch_size(std::stoi(config.at(InferenceBackend::KEY_BASE).at(InferenceBackend::KEY_BATCH_SIZE))),
          model_path(model_path) {
    }

    std::unique_ptr<InferenceBackend::ModelLoader> loader;
    const std::map<std::string, std::string> base_config;
    const std::map<std::string, std::string> inference_config;
    const std::map<std::string, std::string> layer_precision_config;
    const std::map<std::string, std::string> layer_type_config;
    const size_t batch_size;
    const std::string model_path;
};

struct IrBuilder : EntityBuilder {
    IrBuilder(const std::map<std::string, std::map<std::string, std::string>> &config, const std::string &model_path,
              void *display = nullptr)
        : EntityBuilder(std::unique_ptr<InferenceBackend::ModelLoader>(new InferenceBackend::IrModelLoader(display)),
                        config, model_path) {
    }

  private:
    void configureNetworkLayers(const InferenceEngine::InputsDataMap &inputs_info, std::string &image_input_name);
    std::tuple<std::unique_ptr<InferenceBackend::ImagePreprocessor>, InferenceEngine::ExecutableNetwork, std::string>
    createPreProcAndExecutableNetwork_impl(InferenceEngine::CNNNetwork &network, InferenceEngine::Core &core) override;
};

struct CompiledBuilder : EntityBuilder {
    CompiledBuilder(const std::map<std::string, std::map<std::string, std::string>> &config,
                    const std::string &model_path)
        : EntityBuilder(std::unique_ptr<InferenceBackend::ModelLoader>(new InferenceBackend::CompiledModelLoader()),
                        config, model_path) {
    }

  private:
    std::tuple<std::unique_ptr<InferenceBackend::ImagePreprocessor>, InferenceEngine::ExecutableNetwork, std::string>
    createPreProcAndExecutableNetwork_impl(InferenceEngine::CNNNetwork &network, InferenceEngine::Core &core);
};
