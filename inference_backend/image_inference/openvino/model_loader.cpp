/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "model_loader.h"
#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "utils.h"

#include <ie_compound_blob.h>
#include <inference_engine.hpp>
#include <ngraph/ngraph.hpp>

#ifdef ENABLE_VAAPI
#include <cldnn/cldnn_config.hpp>
#include <gpu/gpu_context_api_va.hpp>
#endif

#include <fstream>

using namespace InferenceBackend;

namespace {

bool IsNetworkWithDynamicInputShapes(InferenceEngine::CNNNetwork &network) {
    InferenceEngine::ICNNNetwork::InputShapes input_shapes = network.getInputShapes();
    if (input_shapes.empty())
        throw std::invalid_argument("There are no input shapes");

    for (const auto &input_shape : input_shapes) {
        if (input_shape.second.empty())
            return true;
    }

    return false;
}

inline bool isReshapeNeeded(bool reshape, size_t batch_size, size_t reshape_width, size_t reshape_height) {
    return ((reshape) && ((batch_size > 1) || reshape_width || reshape_height));
}

void FillInputShape(InferenceEngine::SizeVector &input_shape, InferenceEngine::Layout layout, size_t batch_size,
                    size_t width, size_t height) {
    size_t batch_index, width_index, height_index;

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
        throw std::runtime_error("Unsupported InferenceEngine::Layout format for network reshape: " +
                                 std::to_string(layout));
    }

    input_shape[batch_index] = batch_size;
    if (height > 0)
        input_shape[height_index] = height;
    if (width > 0)
        input_shape[width_index] = width;
}

std::tuple<size_t, size_t> GetDimsFromInputDynamicShape(InferenceEngine::CNNNetwork &network) {
    const auto parameters = network.getFunction()->get_parameters();
    if (parameters.empty())
        throw std::runtime_error("Failed to get 'ngraph::ParameterVector' from the network");

    // TODO: support for models with multiple inputs
    if (parameters.size() > 1)
        throw std::runtime_error("Models with multiple dynamic input shapes are not supported");

    ngraph::PartialShape part_shape = parameters[0]->get_partial_shape();
    if (part_shape.rank().is_dynamic())
        throw std::runtime_error("Can't processing " + network.getName() +
                                 " network with all dynamic dimensions in input shape. Specify the input dimensions in "
                                 "'batch-size', 'reshape-width' and 'reshape-height' parameters");

    size_t height = part_shape[2].get_length();
    size_t width = part_shape[3].get_length();

    return std::make_tuple(width, height);
}

void MakeNetworkInputShapesStatic(InferenceEngine::CNNNetwork &network, size_t batch_size, size_t width,
                                  size_t height) {
    if (!width || !height)
        std::tie(width, height) = GetDimsFromInputDynamicShape(network);

    InferenceEngine::ICNNNetwork::InputShapes input_shapes = network.getInputShapes();
    if (input_shapes.empty())
        throw std::invalid_argument("There are no input shapes");

    const size_t channels_num = 3;
    std::string input_name = input_shapes.begin()->first;
    input_shapes[input_name] = {batch_size, channels_num, height, width};
    network.reshape(input_shapes);
}

void ReshapeNetwork(InferenceEngine::CNNNetwork &network, size_t batch_size, size_t width = 0, size_t height = 0) {
    try {
        InferenceEngine::ICNNNetwork::InputShapes input_shapes = network.getInputShapes();
        if (input_shapes.empty())
            throw std::invalid_argument("There are no input shapes");

        // TODO: support for models with multiple inputs
        if (input_shapes.size() > 1)
            throw std::runtime_error("Reshape does not support models with multiple input shapes");

        std::string input_name;
        InferenceEngine::SizeVector input_shape;
        std::tie(input_name, input_shape) = *input_shapes.begin();

        InferenceEngine::InputsDataMap inputs = network.getInputsInfo();
        if (inputs.empty())
            throw std::invalid_argument("Input layers info is absent for model");

        InferenceEngine::InputInfo::Ptr &input = inputs.begin()->second;
        InferenceEngine::Layout layout = input->getInputData()->getLayout();
        FillInputShape(input_shape, layout, batch_size, width, height);

        input_shapes[input_name] = input_shape;
        network.reshape(input_shapes);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to reshape network '" + network.getName() + "'"));
    }
}

inline std::string fileNameNoExt(const std::string &filepath) {
    auto pos = filepath.rfind('.');
    if (pos == std::string::npos)
        return filepath;
    return filepath.substr(0, pos);
}

inline std::string fileExt(const std::string &filepath) {
    auto pos = filepath.rfind('.');
    if (pos == std::string::npos)
        return std::string{};
    return filepath.substr(pos, filepath.size());
}

} // namespace

bool ModelLoader::is_valid_model_path(const std::string &model_path) {
    if (!Utils::fileExists(model_path)) {
        return false;
    }

    const auto ext = fileExt(model_path);
    if (ext.empty()) {
        return false;
    }

    const std::array<std::string, 3> supported_model_file_types = {".xml", ".blob", ".onnx"};
    auto ext_it = std::find(supported_model_file_types.cbegin(), supported_model_file_types.cend(), ext);
    if (ext_it != supported_model_file_types.cend()) {
        if (ext == ".xml") {
            const std::string model_bin = fileNameNoExt(model_path) + ".bin";
            return Utils::fileExists(model_bin);
        }
        return true;
    }
    return false;
}

bool ModelLoader::is_compile_model(const std::string &model_path) {
    bool is_compile = false;
    if (fileExt(model_path) == ".blob") {
        is_compile = true;
    }
    return is_compile;
}

InferenceEngine::CNNNetwork IrModelLoader::load(InferenceEngine::Core &core, const std::string &model,
                                                const std::map<std::string, std::string> &base_config) {
    try {
        // Load IR or ONNX network (.xml, .onnx file)
        InferenceEngine::CNNNetwork network = core.ReadNetwork(model);
        bool reshape =
            (base_config.find(KEY_RESHAPE) != base_config.end()) ? std::stoi(base_config.at(KEY_RESHAPE)) : false;
        size_t batch_size =
            (base_config.find(KEY_BATCH_SIZE) != base_config.end()) ? std::stoul(base_config.at(KEY_BATCH_SIZE)) : 1;
        size_t reshape_width = (base_config.find(KEY_RESHAPE_WIDTH) != base_config.end())
                                   ? std::stoul(base_config.at(KEY_RESHAPE_WIDTH))
                                   : 0;
        size_t reshape_height = (base_config.find(KEY_RESHAPE_HEIGHT) != base_config.end())
                                    ? std::stoul(base_config.at(KEY_RESHAPE_HEIGHT))
                                    : 0;

        if (IsNetworkWithDynamicInputShapes(network))
            MakeNetworkInputShapesStatic(network, batch_size, reshape_width, reshape_height);
        else if (isReshapeNeeded(reshape, batch_size, reshape_width, reshape_height))
            ReshapeNetwork(network, batch_size, reshape_width, reshape_height);

        return network;
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to load model '" + model + "'"));
    }
    return InferenceEngine::CNNNetwork();
}

std::string IrModelLoader::name(const NetworkReferenceWrapper &network) {
    return network.getCNN().getName();
}

InferenceEngine::ExecutableNetwork IrModelLoader::import(InferenceEngine::CNNNetwork &network, const std::string &,
                                                         InferenceEngine::Core &core,
                                                         const std::map<std::string, std::string> &base_config,
                                                         const std::map<std::string, std::string> &inference_config) {

    if (base_config.count(KEY_DEVICE) == 0)
        throw std::runtime_error("Device does not specified");
    const std::string &device = base_config.at(KEY_DEVICE);
    InferenceEngine::ExecutableNetwork executable_network;

#if defined(ENABLE_VAAPI)
    if (_display and device == "GPU") {
        InferenceEngine::gpu::VAContext::Ptr context =
            InferenceEngine::gpu::make_shared_context(core, device, _display);
        // This is a temporary workround to provide a compound blob instead of a remote one
        std::map<std::string, std::string> inference_config_ = inference_config;
        inference_config_[InferenceEngine::CLDNNConfigParams::KEY_CLDNN_NV12_TWO_INPUTS] =
            InferenceEngine::PluginConfigParams::YES;
        // Surface sharing works only with GPU_THROUGHPUT_STREAMS equal to default value ( = 1)
        inference_config_.erase("GPU_THROUGHPUT_STREAMS");
        executable_network = core.LoadNetwork(network, context, inference_config_);
    } else {
        executable_network = core.LoadNetwork(network, device, inference_config);
    }
#else
    executable_network = core.LoadNetwork(network, device, inference_config);
#endif
    return executable_network;
}

InferenceEngine::CNNNetwork CompiledModelLoader::load(InferenceEngine::Core &, const std::string &,
                                                      const std::map<std::string, std::string> &) {
    return InferenceEngine::CNNNetwork();
}

std::string CompiledModelLoader::name(const NetworkReferenceWrapper &network) {
    return network.getExecutable().GetMetric(EXEC_NETWORK_METRIC_KEY(NETWORK_NAME));
}

InferenceEngine::ExecutableNetwork
CompiledModelLoader::import(InferenceEngine::CNNNetwork &, const std::string &model, InferenceEngine::Core &core,
                            const std::map<std::string, std::string> &base_config,
                            const std::map<std::string, std::string> &inference_config) {
    try {
        InferenceEngine::ExecutableNetwork executable_network;
        if (base_config.count(KEY_DEVICE) == 0)
            throw std::invalid_argument("Inference device is not specified");
        const std::string &device = base_config.at(KEY_DEVICE);
        executable_network = core.ImportNetwork(model, device, inference_config);
        return executable_network;
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to import pre-compiled model"));
    }
}
