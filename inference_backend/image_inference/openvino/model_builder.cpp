/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "model_builder.h"

#include "inference_backend/logger.h"

namespace {
void addExtension(InferenceEngine::Core &core, const std::map<std::string, std::string> &base_config);
InferenceEngine::Precision getIePrecision(const std::string &str);
InferenceEngine::InputsDataMap modelInputsInfo(InferenceEngine::ExecutableNetwork &executable_network);
InferenceEngine::ColorFormat formatNameToIEColorFormat(const std::string &format);
std::unique_ptr<InferenceBackend::ImagePreprocessor>
createPreProcessor(InferenceEngine::InputInfo::Ptr input, size_t batch_size,
                   const std::map<std::string, std::string> &base_config);
} // namespace

void IrBuilder::configureNetworkLayers(const InferenceEngine::InputsDataMap &inputs_info,
                                       std::string &image_input_name) {
    if (inputs_info.size() == 0)
        std::invalid_argument("Network inputs info is empty");

    if (inputs_info.size() == 1) {
        auto info = inputs_info.begin();
        GVA_INFO(std::string("Input image layer name: \"" + info->first + "\"").c_str());
        const auto precision_it = layer_precision_config.find(info->first);
        if (precision_it == layer_precision_config.cend()) {
            info->second->setPrecision(InferenceEngine::Precision::U8);
        } else {
            info->second->setPrecision(getIePrecision(precision_it->second));
        }
        image_input_name = info->first;
    } else {
        for (auto &input_info : inputs_info) {
            auto precision_it = layer_precision_config.find(input_info.first);
            auto type_it = layer_type_config.find(input_info.first);
            if (precision_it == layer_precision_config.end() or type_it == layer_type_config.end())
                throw std::invalid_argument("Config for layer precision does not contain precision info for layer: " +
                                            input_info.first);
            if (type_it->second == InferenceBackend::KEY_image)
                image_input_name = input_info.first;
            input_info.second->setPrecision(getIePrecision(precision_it->second));
        }
    }
}

std::tuple<std::unique_ptr<InferenceBackend::ImagePreprocessor>, InferenceEngine::ExecutableNetwork, std::string>
IrBuilder::createPreProcAndExecutableNetwork_impl(InferenceEngine::CNNNetwork &network, InferenceEngine::Core &core) {
    addExtension(core, base_config);
    auto inputs_info = network.getInputsInfo();
    std::string image_input_name;
    configureNetworkLayers(inputs_info, image_input_name);
    std::unique_ptr<InferenceBackend::ImagePreprocessor> pre_processor =
        createPreProcessor(inputs_info[image_input_name], batch_size, base_config);
    InferenceEngine::ExecutableNetwork executable_network =
        loader->import(network, model_path, core, base_config, inference_config);
    return std::make_tuple<std::unique_ptr<InferenceBackend::ImagePreprocessor>, InferenceEngine::ExecutableNetwork,
                           std::string>(std::move(pre_processor), std::move(executable_network),
                                        std::move(image_input_name));
}

std::tuple<std::unique_ptr<InferenceBackend::ImagePreprocessor>, InferenceEngine::ExecutableNetwork, std::string>
CompiledBuilder::createPreProcAndExecutableNetwork_impl(InferenceEngine::CNNNetwork &network,
                                                        InferenceEngine::Core &core) {
    InferenceEngine::ExecutableNetwork executable_network =
        loader->import(network, model_path, core, base_config, inference_config);

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
void addExtension(InferenceEngine::Core &core, const std::map<std::string, std::string> &base_config) {
    if (base_config.count(InferenceBackend::KEY_CPU_EXTENSION)) {
        try {
            const std::string &cpu_extension = base_config.at(InferenceBackend::KEY_CPU_EXTENSION);
            const auto extension_ptr = InferenceEngine::make_so_pointer<InferenceEngine::IExtension>(cpu_extension);
            core.AddExtension(extension_ptr, "CPU");
        } catch (const std::exception &e) {
            std::throw_with_nested(std::runtime_error("Failed to add CPU extension"));
        }
    }
    if (base_config.count(InferenceBackend::KEY_GPU_EXTENSION)) {
        try {
            // TODO is core.setConfig() same with inference_config
            const std::string &config_file = base_config.at(InferenceBackend::KEY_GPU_EXTENSION);
            core.SetConfig({{InferenceEngine::PluginConfigParams::KEY_CONFIG_FILE, config_file}}, "GPU");
        } catch (const std::exception &e) {
            std::throw_with_nested(std::runtime_error("Failed to add GPU extension"));
        }
    }
    if (base_config.count(InferenceBackend::KEY_VPU_EXTENSION)) {
        try {
            const std::string &config_file = base_config.at(InferenceBackend::KEY_VPU_EXTENSION);
            core.SetConfig({{InferenceEngine::PluginConfigParams::KEY_CONFIG_FILE, config_file}}, "VPU");
        } catch (const std::exception &e) {
            std::throw_with_nested(std::runtime_error("Failed to add VPU extension"));
        }
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
        std::string err =
            "Color format '" + format +
            "' is not supported by Inference Engine preprocessing. InferenceEngine::ColorFormat::RAW will be set";
        GVA_ERROR(err.c_str());
        return InferenceEngine::ColorFormat::RAW;
    }
}

std::unique_ptr<InferenceBackend::ImagePreprocessor>
createPreProcessor(InferenceEngine::InputInfo::Ptr input, size_t batch_size,
                   const std::map<std::string, std::string> &base_config) {
    const std::string &image_format =
        base_config.count(InferenceBackend::KEY_IMAGE_FORMAT) ? base_config.at(InferenceBackend::KEY_IMAGE_FORMAT) : "";
    const std::string &image_preprocessor_type_str = base_config.count(InferenceBackend::KEY_PRE_PROCESSOR_TYPE)
                                                         ? base_config.at(InferenceBackend::KEY_PRE_PROCESSOR_TYPE)
                                                         : "";
    InferenceBackend::ImagePreprocessorType image_preprocessor_type =
        static_cast<InferenceBackend::ImagePreprocessorType>(std::stoi(image_preprocessor_type_str));
    if (not input) {
        throw std::invalid_argument("Inputs is empty");
    }
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
            if (batch_size > 1)
                throw std::runtime_error("Surface sharing with batching is not supported");
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
} // namespace
