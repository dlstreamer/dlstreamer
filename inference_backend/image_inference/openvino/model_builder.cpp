/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "model_builder.h"
#include "inference_backend/logger.h"
#include "utils.h"
#include <core_singleton.h>

namespace {
void addExtension(const std::map<std::string, std::string> &base_config,
                  std::map<std::string, std::string> &inference_config);
InferenceEngine::Precision getIePrecision(const std::string &str);
InferenceEngine::InputsDataMap modelInputsInfo(InferenceEngine::ExecutableNetwork &executable_network);
InferenceEngine::ColorFormat formatNameToIEColorFormat(const std::string &format);
std::unique_ptr<InferenceBackend::ImagePreprocessor>
createPreProcessor(InferenceEngine::InputInfo::Ptr input, size_t batch_size,
                   const std::map<std::string, std::string> &base_config);
void checkLayersExist(const InferenceEngine::InputsDataMap &inputs_info,
                      const std::map<std::string, std::string> &layers_config);
} // namespace

void IrBuilder::configureNetworkLayers(const InferenceEngine::InputsDataMap &inputs_info,
                                       std::string &image_input_name) {
    if (inputs_info.size() == 0)
        std::invalid_argument("Network inputs info is empty");

    if (inputs_info.size() == 1) {
        auto info = inputs_info.begin();
        GVA_INFO("Input image layer name: '%s'", info->first.c_str());
        const auto precision_it = input_layer_precision_config.find(info->first);
        if (precision_it == input_layer_precision_config.cend()) {
            info->second->setPrecision(InferenceEngine::Precision::U8);
        } else {
            info->second->setPrecision(getIePrecision(precision_it->second));
        }
        image_input_name = info->first;
    } else {
        for (auto &input_info : inputs_info) {
            auto precision_it = input_layer_precision_config.find(input_info.first);
            auto type_it = layer_format_config.find(input_info.first);
            if (precision_it == input_layer_precision_config.end() or type_it == layer_format_config.end())
                throw std::invalid_argument("Config for layer precision does not contain precision info for layer: " +
                                            input_info.first);
            if (type_it->second == InferenceBackend::KEY_image)
                image_input_name = input_info.first;
            input_info.second->setPrecision(getIePrecision(precision_it->second));
        }
    }
}

void IrBuilder::checkLayersConfig(const InferenceEngine::InputsDataMap &inputs_info) {
    checkLayersExist(inputs_info, input_layer_precision_config);
    checkLayersExist(inputs_info, layer_format_config);
}

std::tuple<std::unique_ptr<InferenceBackend::ImagePreprocessor>, InferenceEngine::ExecutableNetwork, std::string>
IrBuilder::createPreProcAndExecutableNetwork_impl(InferenceEngine::CNNNetwork &network) {
    addExtension(base_config, inference_config);
    auto inputs_info = network.getInputsInfo();

    checkLayersConfig(inputs_info);

    std::string image_input_name;
    configureNetworkLayers(inputs_info, image_input_name);

    std::unique_ptr<InferenceBackend::ImagePreprocessor> pre_processor =
        createPreProcessor(inputs_info[image_input_name], batch_size, base_config);

    InferenceEngine::ExecutableNetwork executable_network =
        loader->import(network, model_path, base_config, inference_config);

    return std::make_tuple<std::unique_ptr<InferenceBackend::ImagePreprocessor>, InferenceEngine::ExecutableNetwork,
                           std::string>(std::move(pre_processor), std::move(executable_network),
                                        std::move(image_input_name));
}

std::tuple<std::unique_ptr<InferenceBackend::ImagePreprocessor>, InferenceEngine::ExecutableNetwork, std::string>
CompiledBuilder::createPreProcAndExecutableNetwork_impl(InferenceEngine::CNNNetwork &network) {
    InferenceEngine::ExecutableNetwork executable_network =
        loader->import(network, model_path, base_config, inference_config);

    auto inputs_info = modelInputsInfo(executable_network);
    if (inputs_info.size() > 1)
        throw std::runtime_error("Not supported models with many inputs");
    auto info = inputs_info.begin();
    InferenceEngine::InputInfo::Ptr image_input = info->second;
    std::string image_input_name = info->first;
    std::unique_ptr<InferenceBackend::ImagePreprocessor> pre_processor =
        createPreProcessor(image_input, batch_size, base_config);
    return std::make_tuple<std::unique_ptr<InferenceBackend::ImagePreprocessor>, InferenceEngine::ExecutableNetwork,
                           std::string>(std::move(pre_processor), std::move(executable_network),
                                        std::move(image_input_name));
}

namespace {
void addExtension(const std::map<std::string, std::string> &base_config,
                  std::map<std::string, std::string> &inference_config) {
    std::map<std::string, std::string> extensions =
        Utils::stringToMap(base_config.at(InferenceBackend::KEY_DEVICE_EXTENSIONS));
    const std::string &device = base_config.at(InferenceBackend::KEY_DEVICE);

    // 1. Process CPU extension first, because API differs from other devices extension. CPU extensions are shared
    // across GVA elements, which use CPU or add CPU extensions. Thus, these extensions are set per process (NOT per GVA
    // element/DL model)
    if (extensions.count("CPU")) {
        try {
            IeCoreSingleton::Instance().AddExtension(
                InferenceEngine::make_so_pointer<InferenceEngine::IExtension>(extensions["CPU"]), "CPU");
        } catch (const std::exception &e) {
            std::throw_with_nested(std::runtime_error("Failed to add CPU extension: " + extensions["CPU"]));
        }
        extensions.erase("CPU"); // this extension was processed, don't want to load CPU extension again
    }
    // 2. Process HETERO and MULTI devices, in this case gva_base_inference->device_extensions is
    // "GPU=extension1,CPU=extension2,MYRIAD=extension3"-like string. In this case we can't set extensions per model
    // (through LoadNetwork/ImportNetwork), because we can't restrict model execution to one particular device
    if (device.rfind("HETERO", 0) == 0 || device.rfind("MULTI", 0) == 0) {
        for (auto it = extensions.begin(); it != extensions.end(); ++it) {
            try {
                const std::string &device_under_extension = it->first;
                const std::string &config_file = it->second;
                if (device.find(device_under_extension) == std::string::npos)
                    throw std::runtime_error("Device " + device + " does not contain " + device_under_extension + ". " +
                                             device_under_extension + " extension can't be applied");
                // Process non-CPU device extension. Non-CPU devices can be extended following way (through
                // global config). These extensions are additive (different custom layers coming from different
                // config_file will be merged after all), and set per process per device (NOT per GVA element/DL model)
                IeCoreSingleton::Instance().SetConfig(
                    {{InferenceEngine::PluginConfigParams::KEY_CONFIG_FILE, config_file}}, device_under_extension);
            } catch (const std::exception &e) {
                std::throw_with_nested(std::runtime_error("Failed to add " + it->first + " extension: " + it->second));
            }
        }
    }
    // 3. Process single non-CPU device. In this case gva_base_inference->device_extensions is
    // "<device>=single_extension" string
    else {
        if (extensions.count(device)) {
            // This extension will be set for this device for particular model. It means,
            // different extensions can be set per process per device per each GVA element/DL model
            inference_config[InferenceEngine::PluginConfigParams::KEY_CONFIG_FILE] = extensions[device];
            extensions.erase(device); // this extension was processed
        }
        if (!extensions.empty())
            throw std::runtime_error("Device extension " + extensions.begin()->first +
                                     " can't be applied to chosen inference device: " + device);
    }
}

InferenceEngine::Precision getIePrecision(const std::string &str) {
    InferenceEngine::Precision prec = InferenceEngine::Precision::FP32;
    if (str == "U8") {
        prec = InferenceEngine::Precision::U8;
    } else if (str != "FP32") {
        throw std::runtime_error("Unsupported input_layer precision: " + str);
    }
    return prec;
}

InferenceEngine::InputsDataMap modelInputsInfo(InferenceEngine::ExecutableNetwork &executable_network) {
    InferenceEngine::ConstInputsDataMap const_model_inputs_info = executable_network.GetInputsInfo();
    InferenceEngine::InputsDataMap model_inputs_info;

    // Workaround IE API to fill model_inputs_info with mutable pointers
    std::for_each(const_model_inputs_info.begin(), const_model_inputs_info.end(),
                  [&](std::pair<const std::string, InferenceEngine::InputInfo::CPtr> pair) {
                      model_inputs_info.emplace(pair.first,
                                                std::const_pointer_cast<InferenceEngine::InputInfo>(pair.second));
                  });
    return model_inputs_info;
}

InferenceEngine::ColorFormat formatNameToIEColorFormat(const std::string &format) {
    static const std::map<std::string, InferenceEngine::ColorFormat> formatMap{
        {"NV12", InferenceEngine::ColorFormat::NV12}, {"I420", InferenceEngine::ColorFormat::I420},
        {"RGB", InferenceEngine::ColorFormat::RGB},   {"BGR", InferenceEngine::ColorFormat::BGR},
        {"RGBX", InferenceEngine::ColorFormat::RGBX}, {"BGRX", InferenceEngine::ColorFormat::BGRX},
        {"RGBA", InferenceEngine::ColorFormat::RGBX}, {"BGRA", InferenceEngine::ColorFormat::BGRX}};
    auto iter = formatMap.find(format);
    if (iter != formatMap.end()) {
        return iter->second;
    } else {
        GVA_ERROR("Color format '%s' is not supported by Inference Engine preprocessing. "
                  "InferenceEngine::ColorFormat::RAW will be set",
                  format.c_str());
        return InferenceEngine::ColorFormat::RAW;
    }
}

std::unique_ptr<InferenceBackend::ImagePreprocessor>
createPreProcessor(InferenceEngine::InputInfo::Ptr input, size_t batch_size,
                   const std::map<std::string, std::string> &base_config) {
    if (not input)
        throw std::invalid_argument("Inputs are empty");

    const std::string &image_format =
        base_config.count(InferenceBackend::KEY_IMAGE_FORMAT) ? base_config.at(InferenceBackend::KEY_IMAGE_FORMAT) : "";
    const std::string &image_preprocessor_type_str = base_config.count(InferenceBackend::KEY_PRE_PROCESSOR_TYPE)
                                                         ? base_config.at(InferenceBackend::KEY_PRE_PROCESSOR_TYPE)
                                                         : "";
    InferenceBackend::ImagePreprocessorType image_preprocessor_type =
        static_cast<InferenceBackend::ImagePreprocessorType>(std::stoi(image_preprocessor_type_str));
    const std::string &device = base_config.at(InferenceBackend::KEY_DEVICE);

    try {
        std::unique_ptr<InferenceBackend::ImagePreprocessor> pre_processor = nullptr;
        switch (image_preprocessor_type) {
        case InferenceBackend::ImagePreprocessorType::IE:
            if (batch_size > 1)
                throw std::runtime_error("Inference Engine preprocessing with batching is not supported");
            input->getPreProcess().setResizeAlgorithm(InferenceEngine::ResizeAlgorithm::RESIZE_BILINEAR);
            input->getPreProcess().setColorFormat(formatNameToIEColorFormat(image_format));
            break;
        case InferenceBackend::ImagePreprocessorType::VAAPI_SURFACE_SHARING:
            if (device.find("GPU") == std::string::npos)
                throw std::runtime_error("Surface sharing is supported only on GPU device plugin");
            input->setLayout(InferenceEngine::Layout::NCHW);
            input->setPrecision(InferenceEngine::Precision::U8);
            input->getPreProcess().setColorFormat(InferenceEngine::ColorFormat::NV12);
            break;
        case InferenceBackend::ImagePreprocessorType::OPENCV:
        case InferenceBackend::ImagePreprocessorType::VAAPI_SYSTEM:
            pre_processor.reset(InferenceBackend::ImagePreprocessor::Create(image_preprocessor_type));
            break;
        default:
            throw std::invalid_argument("Image preprocessor is not implemented");
        }
        return pre_processor;
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to create preprocessor of"));
    }
}

void checkLayersExist(const InferenceEngine::InputsDataMap &inputs_info,
                      const std::map<std::string, std::string> &layers_config) {
    for (const auto &lp : layers_config) {
        const auto &layer_name = lp.first;
        if (layer_name != "ANY" && inputs_info.find(layer_name) == inputs_info.cend()) {
            throw std::runtime_error("Layer '" + layer_name +
                                     "' does not exist. Please, check `input_preproc` section in model-proc.");
        }
    }
}
} // namespace
