/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference.h"
#include "core_singleton.h"
#include "image_inference.h"
#include "inference_backend/logger.h"
#include "model_loader.h"
#include "utils.h"

#include <algorithm>
#include <fstream>
#include <limits.h>
#include <string>

#ifdef ENABLE_VPUX
#include <vpux/kmb_params.hpp>
#endif

using namespace InferenceEngine;
using namespace std;
using namespace InferenceBackend;

namespace {
class IEOutputBlob : public OutputBlob {
  public:
    IEOutputBlob(InferenceEngine::Blob::Ptr blob) : blob(blob) {
    }

    const std::vector<size_t> &GetDims() const final {
        return blob->getTensorDesc().getDims();
    }

    Layout GetLayout() const final {
        return static_cast<Layout>(static_cast<int>(blob->getTensorDesc().getLayout()));
    }

    Precision GetPrecision() const final {
        return static_cast<Precision>(static_cast<int>(blob->getTensorDesc().getPrecision()));
    }

    const void *GetData() const final {
        return blob->buffer();
    }

    ~IEOutputBlob() final {
    }

  protected:
    InferenceEngine::Blob::Ptr blob;
};
} // namespace

OpenVINOAudioInference::~OpenVINOAudioInference() {
}

OpenVINOAudioInference::OpenVINOAudioInference(const char *model, char *device, AudioInferenceOutput &infOutput) {

    std::map<std::string, std::string> base;
    std::map<std::string, std::string> inference_config;
    base[KEY_DEVICE] = device;

    if (!InferenceBackend::ModelLoader::is_valid_model_path(model))
        throw std::runtime_error("Invalid model path.");

    auto loader = InferenceBackend::ModelLoader::is_compile_model(model)
                      ? std::unique_ptr<InferenceBackend::ModelLoader>(new CompiledModelLoader())
                      : std::unique_ptr<InferenceBackend::ModelLoader>(new IrModelLoader());
    CNNNetwork network = loader->load(model, base);
    ExecutableNetwork executable_network = loader->import(network, model, base, inference_config);
    InferenceBackend::NetworkReferenceWrapper network_ref(network, executable_network);
    infOutput.model_name = loader->name(network_ref);

    input_name = executable_network.GetInputsInfo().begin()->first;
    inferRequest = executable_network.CreateInferRequest();
    tensor_desc = executable_network.GetInputsInfo().begin()->second->getTensorDesc();

    CreateRemoteContext(base[KEY_DEVICE]);

    InferenceEngine::ConstOutputsDataMap outputs = executable_network.GetOutputsInfo();
    std::map<std::string, std::pair<OutputBlob::Ptr, int>> output_blobs;
    for (auto output : outputs) {
        const std::string &name = output.first;
        InferenceEngine::Blob::Ptr blob = inferRequest.GetBlob(name);
        output_blobs.insert({name, make_pair(std::make_shared<IEOutputBlob>(blob), blob->size())});
    }
    infOutput.output_blobs = output_blobs;
    infOut = infOutput;
}

std::vector<uint8_t> OpenVINOAudioInference::convertFloatToU8(std::vector<float> &normalized_samples) {
    if (normalized_samples.empty())
        throw std::invalid_argument("Invalid Input buffer");
    switch (tensor_desc.getPrecision()) {
    case InferenceEngine::Precision::U8: {
        std::vector<uint8_t> data_after_fq(normalized_samples.size());
        transform(normalized_samples.begin(), normalized_samples.end(), data_after_fq.begin(), [](float v) {
            float fq = ((v - FQ_PARAMS_MIN) / FQ_PARAMS_SCALE) * 255;
            fq = std::max(0.f, std::min(255.f, fq));
            return fq;
        });
        return data_after_fq;
    }
    case InferenceEngine::Precision::FP32:
    case InferenceEngine::Precision::FP16:
        return {};
    default:
        throw std::invalid_argument(std::to_string(tensor_desc.getPrecision()) + " is not supported");
    }
}

void OpenVINOAudioInference::setInputBlob(void *buffer_ptr, int dma_fd) {
    if (!buffer_ptr)
        throw std::invalid_argument("Invalid Input buffer");
    InferenceEngine::Blob::Ptr blob;

#ifdef ENABLE_VPUX
    if (remote_context) {
        ParamMap params = {{InferenceEngine::KMB_PARAM_KEY(REMOTE_MEMORY_FD), dma_fd},
                           {InferenceEngine::KMB_PARAM_KEY(MEM_HANDLE), buffer_ptr}};
        switch (tensor_desc.getPrecision()) {
        case InferenceEngine::Precision::U8:
        case InferenceEngine::Precision::FP16:
        case InferenceEngine::Precision::FP32: {
            blob = remote_context->CreateBlob(tensor_desc, params);
            if (blob == nullptr)
                throw std::runtime_error("Failed to create remote blob for InferenceEngine::Precision " +
                                         std::to_string(tensor_desc.getPrecision()));
            break;
        }
        default:
            throw std::invalid_argument("Failed to create Blob: InferenceEngine::Precision " +
                                        std::to_string(tensor_desc.getPrecision()) + " is not supported");
        }
    } else {
#else
    UNUSED(dma_fd);
#endif
        switch (tensor_desc.getPrecision()) {
        case InferenceEngine::Precision::U8:
            blob = InferenceEngine::make_shared_blob<uint8_t>(tensor_desc, reinterpret_cast<uint8_t *>(buffer_ptr));
            break;
        case InferenceEngine::Precision::FP32:
        case InferenceEngine::Precision::FP16:
            blob = InferenceEngine::make_shared_blob<float>(tensor_desc, reinterpret_cast<float *>(buffer_ptr));
            break;
        default:
            throw std::invalid_argument("Failed to create Blob: InferenceEngine::Precision " +
                                        std::to_string(tensor_desc.getPrecision()) + " is not supported");
        }
#ifdef ENABLE_VPUX
    }
#endif

    inferRequest.SetBlob(input_name, blob);
}

AudioInferenceOutput *OpenVINOAudioInference::getInferenceOutput() {
    return &infOut;
}

void OpenVINOAudioInference::infer() {
    inferRequest.Infer();
}

#ifdef ENABLE_VPUX
void OpenVINOAudioInference::CreateRemoteContext(const std::string &device) {
    std::string vpu_device_name;
    bool has_vpu_device_id = false;
    std::tie(has_vpu_device_id, vpu_device_name) = Utils::parseDeviceName(device);
    if (!vpu_device_name.empty()) {
        const std::string msg = "VPUX device defined as " + vpu_device_name;
        GVA_INFO(msg.c_str());

        const std::string base_device = "VPUX";
        std::string device = vpu_device_name;
        if (!has_vpu_device_id) {
            // Retrieve ID of the first available device
            std::vector<std::string> device_list =
                IeCoreSingleton::Instance().GetMetric(base_device, METRIC_KEY(AVAILABLE_DEVICES));
            if (!device_list.empty())
                device = device_list.at(0);
            // else device is already set to VPU-0
        }

        const InferenceEngine::ParamMap params = {{InferenceEngine::KMB_PARAM_KEY(DEVICE_ID), device}};
        remote_context = IeCoreSingleton::Instance().CreateContext(base_device, params);
    }
}
#else
void OpenVINOAudioInference::CreateRemoteContext(const std::string & /* device */) {
}
#endif
