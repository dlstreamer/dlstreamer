/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference.h"
#include "image_inference.h"
#include "model_loader.h"
#include "utils.h"
#include <fstream>
#include <limits.h>

using namespace InferenceEngine;
using namespace std;
using namespace InferenceBackend;

class IEOutputBlob : public OutputBlob {
  public:
    IEOutputBlob(InferenceEngine::Blob::Ptr blob) : blob(blob) {
    }

    virtual const std::vector<size_t> &GetDims() const {
        return blob->getTensorDesc().getDims();
    }

    virtual Layout GetLayout() const {
        return static_cast<Layout>((int)blob->getTensorDesc().getLayout());
    }

    virtual Precision GetPrecision() const {
        return static_cast<Precision>((int)blob->getTensorDesc().getPrecision());
    }

    virtual const void *GetData() const {
        return blob->buffer();
    }

    virtual ~IEOutputBlob() {
    }

  protected:
    InferenceEngine::Blob::Ptr blob;
};

OpenVINOAudioInference::~OpenVINOAudioInference() {
}

OpenVINOAudioInference::OpenVINOAudioInference(const char *model, char *device, AudioInferenceOutput &infOutput) {

    std::map<std::string, std::string> base;
    std::map<std::string, std::string> inference_config;
    base[KEY_DEVICE] = device;

    auto loader = InferenceBackend::ModelLoader::is_ir_model(model)
                      ? std::unique_ptr<InferenceBackend::ModelLoader>(new IrModelLoader())
                      : std::unique_ptr<InferenceBackend::ModelLoader>(new CompiledModelLoader());
    Core core;
    CNNNetwork network = loader->load(core, model, base);
    ExecutableNetwork executable_network = loader->import(network, model, core, base, inference_config);
    infOutput.model_name = loader->name(network);

    input_name = executable_network.GetInputsInfo().begin()->first;
    inferRequest = executable_network.CreateInferRequest();
    tensor_desc = executable_network.GetInputsInfo().begin()->second->getTensorDesc();

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
            if (fq > 255)
                fq = 255.0;
            if (fq < 0)
                fq = 0.0;
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

void OpenVINOAudioInference::setInputBlob(void *buffer_ptr) {

    if (!buffer_ptr)
        throw std::invalid_argument("Invalid Input buffer");
    InferenceEngine::Blob::Ptr blob;
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
    inferRequest.SetBlob(input_name, blob);
}

AudioInferenceOutput *OpenVINOAudioInference::getInferenceOutput() {
    return &infOut;
}

void OpenVINOAudioInference::infer() {
    inferRequest.Infer();
}