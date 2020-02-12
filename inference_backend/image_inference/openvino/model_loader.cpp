/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "model_loader.h"

#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"

#include <fstream>

using namespace InferenceBackend;

namespace {

void ReshapeNetwork(InferenceEngine::CNNNetwork &network, size_t batch_size, size_t width = 0, size_t height = 0) {
    InferenceEngine::ICNNNetwork::InputShapes input_shapes = network.getInputShapes();
    InferenceEngine::InputsDataMap inputs = network.getInputsInfo();
    InferenceEngine::InputInfo::Ptr &input = inputs.begin()->second;
    InferenceEngine::Layout layout = input->getInputData()->getLayout();

    std::string input_name;
    size_t batch_index, width_index, height_index;
    InferenceEngine::SizeVector input_shape;
    std::tie(input_name, input_shape) = *input_shapes.begin();

    switch (layout) {
    case InferenceEngine::Layout::NCHW: {
        batch_index = 0;
        height_index = 2;
        width_index = 3;
        break;
    }
    case InferenceEngine::Layout::NHWC: {
        batch_index = 0;
        height_index = 1;
        width_index = 2;
        break;
    }
    default:
        throw std::runtime_error("Unsuported layout format: " + std::to_string(layout));
    }

    input_shape[batch_index] = batch_size;
    if (height > 0)
        input_shape[height_index] = height;
    if (width > 0)
        input_shape[width_index] = width;
    input_shapes[input_name] = input_shape;
    network.reshape(input_shapes);
}

inline std::string fileNameNoExt(const std::string &filepath) {
    auto pos = filepath.rfind('.');
    if (pos == std::string::npos)
        return filepath;
    return filepath.substr(0, pos);
}

bool file_exists(const std::string &path) {
    return std::ifstream(path).good();
}

} // namespace

bool ModelLoader::is_ir_model(const std::string &model_path) {
    bool is_ir = false;
    if (model_path.find(".xml") != std::string::npos) {
        const std::string model_bin = fileNameNoExt(model_path) + ".bin";
        is_ir = file_exists(model_path) && file_exists(model_bin);
    }
    return is_ir;
}

InferenceEngine::CNNNetwork IrModelLoader::load(InferenceEngine::Core &core, const std::string &model_xml,
                                                const std::map<std::string, std::string> &base_config) {
    try {
        // Load IR network (.xml file)
        InferenceEngine::CNNNetwork network = core.ReadNetwork(model_xml);

        const bool reshape = std::stoi(base_config.at(KEY_RESHAPE));
        if (reshape) {
            ReshapeNetwork(network, std::stoul(base_config.at(KEY_BATCH_SIZE)),
                           std::stoul(base_config.at(KEY_RESHAPE_WIDTH)),
                           std::stoul(base_config.at(KEY_RESHAPE_HEIGHT)));
        }
        return network;
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Error during loading IR model from " + model_xml));
    }
    return InferenceEngine::CNNNetwork();
}

std::string IrModelLoader::name(const InferenceEngine::CNNNetwork &network) {
    return network.getName();
}

InferenceEngine::ExecutableNetwork IrModelLoader::import(InferenceEngine::CNNNetwork &network, const std::string &,
                                                         InferenceEngine::Core &core,
                                                         const std::map<std::string, std::string> &base_config,
                                                         const std::map<std::string, std::string> &inference_config) {
    if (base_config.count(KEY_DEVICE) == 0)
        throw std::runtime_error("Device does not specified");
    const std::string &device = base_config.at(KEY_DEVICE);
    return core.LoadNetwork(network, device, inference_config);
}

InferenceEngine::CNNNetwork CompiledModelLoader::load(InferenceEngine::Core &, const std::string &,
                                                      const std::map<std::string, std::string> &) {
    return InferenceEngine::CNNNetwork();
}

std::string CompiledModelLoader::name(const InferenceEngine::CNNNetwork &) {
    return std::string();
}

InferenceEngine::ExecutableNetwork
CompiledModelLoader::import(InferenceEngine::CNNNetwork &, const std::string &model, InferenceEngine::Core &core,
                            const std::map<std::string, std::string> &base_config,
                            const std::map<std::string, std::string> &inference_config) {
    InferenceEngine::ExecutableNetwork executable_network;
    if (base_config.count(KEY_DEVICE) == 0)
        throw std::runtime_error("Device does not specified");
    const std::string &device = base_config.at(KEY_DEVICE);
    try {
        executable_network = core.ImportNetwork(model, device, inference_config);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Couldn't import pre-compiled model"));
    }
    return executable_network;
}
