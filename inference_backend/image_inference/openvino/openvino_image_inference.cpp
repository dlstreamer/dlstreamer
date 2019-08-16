/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "openvino_image_inference.h"

#include <functional>
#include <stdio.h>
#include <thread>

#include "inference_backend/pre_proc.h"

#include "inference_backend/logger.h"
#include "plugins_holder.h"

#include <ext_list.hpp>

#include <chrono>
#include <ie_compound_blob.h>

// For debuging purposes uncomment the following to lines
// #include <opencv2/opencv.hpp>
// #include <string>

namespace IE = InferenceEngine;

namespace {

inline std::string fileNameNoExt(const std::string &filepath) {
    auto pos = filepath.rfind('.');
    if (pos == std::string::npos)
        return filepath;
    return filepath.substr(0, pos);
}

inline int GetNumberChannels(int format) {
    switch (format) {
    case InferenceBackend::FOURCC_BGRA:
    case InferenceBackend::FOURCC_BGRX:
    case InferenceBackend::FOURCC_RGBA:
    case InferenceBackend::FOURCC_RGBX:
        return 4;
    case InferenceBackend::FOURCC_BGR:
        return 3;
    }
    return 0;
}

IE::ColorFormat FormatNameToIEColorFormat(const std::string &format) {
    static const std::map<std::string, IE::ColorFormat> formatMap{
        {"NV12", IE::ColorFormat::NV12}, {"RGB", IE::ColorFormat::RGB},   {"BGR", IE::ColorFormat::BGR},
        {"RGBX", IE::ColorFormat::RGBX}, {"BGRX", IE::ColorFormat::BGRX}, {"RGBA", IE::ColorFormat::RGBX},
        {"BGRA", IE::ColorFormat::BGRX}};
    auto iter = formatMap.find(format);
    if (iter != formatMap.end()) {
        return iter->second;
    } else {
        GVA_ERROR("Unsupported color format by Inference Engine preprocessing");
        return IE::ColorFormat::RAW;
    }
}

IE::Blob::Ptr WrapImage2Blob(const Image &image) {
    GVA_DEBUG(__FUNCTION__);
    ITT_TASK(__FUNCTION__);
    if (image.format != InferenceBackend::FOURCC_NV12) {
        int channels = GetNumberChannels(image.format);
        if (image.stride[0] != channels * image.width)
            throw std::runtime_error("Doesn't support conversion from not dense image");

        IE::TensorDesc tensor_desc(IE::Precision::U8, {1, (size_t)channels, (size_t)image.height, (size_t)image.width},
                                   IE::Layout::NHWC);
        IE::Blob::Ptr image_blob = IE::make_shared_blob<uint8_t>(tensor_desc, image.planes[0]);

        if (image.rect.width && image.rect.height) {
            IE::ROI crop_roi(
                {0, (size_t)image.rect.x, (size_t)image.rect.y, (size_t)image.rect.width, (size_t)image.rect.height});
            image_blob = IE::make_shared_blob(image_blob, crop_roi);
        }
        return image_blob;
    } else { // InferenceBackend::FOURCC_NV12
        Image cropedImage = ApplyCrop(image);
        const size_t input_width = (size_t)cropedImage.width;
        const size_t input_height = (size_t)cropedImage.height;
        IE::TensorDesc y_desc(IE::Precision::U8, {1, 1, input_height - input_height % 2, input_width - input_width % 2},
                              IE::Layout::NHWC);
        IE::TensorDesc uv_desc(IE::Precision::U8, {1, 2, input_height / 2, input_width / 2}, IE::Layout::NHWC);

        // Create blob for Y plane from raw data
        IE::Blob::Ptr y_blob = IE::make_shared_blob<uint8_t>(y_desc, cropedImage.planes[0]);
        // Create blob for UV plane from raw data
        IE::Blob::Ptr uv_blob = IE::make_shared_blob<uint8_t>(uv_desc, cropedImage.planes[1]);
        // Create NV12Blob from Y and UV blobs
        IE::Blob::Ptr image_blob = IE::make_shared_blob<IE::NV12Blob>(y_blob, uv_blob);
        return image_blob;
    }
}

inline size_t GetTensorSize(InferenceEngine::TensorDesc desc) {
    auto dims = desc.getDims();
    size_t size = 1;
    for (auto dim : dims)
        size *= dim;
    switch (desc.getPrecision()) {
    case InferenceEngine::Precision::U8:
        return size;
    case InferenceEngine::Precision::FP32:
        return size * sizeof(float);
    default:
        throw std::logic_error("Unsupported precision of the layer");
    }
}

inline std::vector<std::string> split(const std::string &s, char delimiter) {
    std::string token;
    std::istringstream tokenStream(s);
    std::vector<std::string> tokens;
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// std::map<std::string, std::string> configured_ie_config(const std::map<std::string, std::string> &config) {
//     std::map<std::string, std::string> ie_config(config);

//     const std::string &cpu_extension = ie_config.count(KEY_CPU_EXTENSION) ? ie_config.at(KEY_CPU_EXTENSION) : "";
//     const std::string &preProcessorName = ie_config.count(KEY_PRE_PROCESSOR_TYPE) ?
//     ie_config.at(KEY_PRE_PROCESSOR_TYPE) : ""; const std::string &imageFormatName = ie_config.count(KEY_IMAGE_FORMAT)
//     ? ie_config.at(KEY_IMAGE_FORMAT) : "";

//     ie_config.erase(KEY_CPU_EXTENSION);
//     ie_config.erase(KEY_PRE_PROCESSOR_TYPE);
//     ie_config.erase(KEY_IMAGE_FORMAT);

//     return std::tie(ie_config, cpu_extension, preProcessorName, imageFormatName);
// }

// InferenceEngine::CNNNetwork create_cnn_network(const std::string &model) {
//     InferenceEngine::CNNNetReader network_reader;
//     network_reader.ReadNetwork(model);
//     network_reader.ReadWeights(fileNameNoExt(model) + ".bin");
//     modelName = network_reader.getName();
//     return network_reader.getNetwork();
// }

} // namespace

//////////////////////////////////////////////////////////////////////////////////

class IEOutputBlob : public OutputBlob {
  public:
    IEOutputBlob(IE::Blob::Ptr blob) : blob(blob) {
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
    IE::Blob::Ptr blob;
};

//////////////////////////////////////////////////////////////////////////////////

OpenVINOImageInference::~OpenVINOImageInference() {
    GVA_DEBUG("Image Inference destruct");
    Close();
}

OpenVINOImageInference::OpenVINOImageInference(std::string devices, std::string model, int batch_size, int nireq,
                                               const std::map<std::string, std::string> &config, Allocator *allocator,
                                               CallbackFunc callback)

    : allocator(allocator), batch_size(batch_size) {

    GVA_DEBUG("Image Inference construct");

    std::atomic_init(&requests_processing_, static_cast<unsigned int>(0));

    auto devices_vec = split(devices, '-');
    const std::string &cpu_extension = config.count(KEY_CPU_EXTENSION) ? config.at(KEY_CPU_EXTENSION) : "";
    const std::string &preProcessorName = config.count(KEY_PRE_PROCESSOR_TYPE) ? config.at(KEY_PRE_PROCESSOR_TYPE) : "";
    const std::string &imageFormatName = config.count(KEY_IMAGE_FORMAT) ? config.at(KEY_IMAGE_FORMAT) : "";
    // TODO: Inference Engine asserts if unknown key passed
    std::map<std::string, std::string> ie_config(config);
    ie_config.erase(KEY_CPU_EXTENSION);
    ie_config.erase(KEY_PRE_PROCESSOR_TYPE);
    ie_config.erase(KEY_IMAGE_FORMAT);

    // Load network (.xml and .bin files)
    InferenceEngine::CNNNetReader network_reader;
    network_reader.ReadNetwork(model);
    network_reader.ReadWeights(fileNameNoExt(model) + ".bin");
    modelName = network_reader.getName();
    InferenceEngine::CNNNetwork network = network_reader.getNetwork();

    // Check model input
    IE::InputsDataMap model_inputs_info(network.getInputsInfo());
    if (model_inputs_info.size() == 0) {
        throw std::logic_error("Input layer not found");
    }
    auto &input = model_inputs_info.begin()->second;
    input->setPrecision(IE::Precision::U8);
    input->getInputData()->setLayout(IE::Layout::NCHW);

    if (preProcessorName.compare("ie") == 0) {
        input->getPreProcess().setResizeAlgorithm(IE::ResizeAlgorithm::RESIZE_BILINEAR);
        input->getPreProcess().setColorFormat(FormatNameToIEColorFormat(imageFormatName));
    } else {
        PreProcessType preProcessorType = PreProcessType::Invalid;
        if (preProcessorName.empty() || preProcessorName.compare("opencv") == 0)
            preProcessorType = PreProcessType::OpenCV;
        else if (preProcessorName.compare("g-api") == 0)
            preProcessorType = PreProcessType::GAPI;
        sw_vpp.reset(PreProc::Create(preProcessorType));
        if (!sw_vpp.get()) {
            // TODO ERROR
        }
    }
    network.setBatchSize(batch_size);

    for (const std::string &device : devices_vec) { // '-' separated list of devices
        IE::InferencePlugin::Ptr plugin;
        try {
            plugin = PluginsHolderSingleton::getInstance().getPluginPtr(device);
        } catch (const std::exception &e) {
            std::throw_with_nested(std::runtime_error("Can not create plugin for device: " + device));
        }
        plugins.push_back(plugin);

        // Extension for custom layers

        if (device.find("CPU") != std::string::npos) {
            // library with custom layers
            plugin->AddExtension(std::make_shared<IE::Extensions::Cpu::CpuExtensions>());

            if (!cpu_extension.empty()) {
                // CPU(MKLDNN) extensions are loaded as a shared library and passed as a pointer to base extension
                auto extension_ptr = IE::make_so_pointer<IE::IExtension>(cpu_extension);
                plugin->AddExtension(extension_ptr);
            }
        }

        // Loading a model to the device
        auto executable_network = plugin->LoadNetwork(network, ie_config);

        inputs = executable_network.GetInputsInfo();
        outputs = executable_network.GetOutputsInfo();
        std::map<std::string, InferenceEngine::TensorDesc> layers;
        for (auto input : inputs) {
            layers[input.first] = input.second->getTensorDesc();
        }
        for (auto output : outputs) {
            layers[output.first] = output.second->getTensorDesc();
        }

        if (nireq == 0) {
            try {
                nireq = executable_network.GetMetric(IE::Metrics::METRIC_OPTIMAL_NUMBER_OF_INFER_REQUESTS)
                            .as<unsigned int>();
                nireq += 1; // One additional for pre-processing parallelization with inference
                std::string msg = "Setting the optimal number of inference request: nireq=" + std::to_string(nireq);
                GVA_WARNING(msg.c_str());
            } catch (const std::exception &e) {
                std::string msg = std::string("Failed to get the optimal number of inference request.\n") + e.what();
                std::throw_with_nested(std::runtime_error(msg));
            }
        }

        for (int i = 0; i < nireq; i++) {
            std::shared_ptr<BatchRequest> req = std::make_shared<BatchRequest>();
            req->infer_request = executable_network.CreateInferRequestPtr();
            auto completion_callback = [this, req]() {
                // TODO: check if mutex is not useless
                std::lock_guard<std::mutex> lock(this->inference_completion_mutex_);
                this->WorkingFunction(req);
                this->requests_processing_ -= this->batch_size;
                this->request_processed_.notify_all();
            };
            req->infer_request->SetCompletionCallback(completion_callback);
            if (allocator) {
                for (auto layer : layers) {
                    size_t nbytes = GetTensorSize(layer.second);
                    void *buffer_ptr = nullptr;
                    InferenceBackend::Allocator::AllocContext *alloc_context = nullptr;
                    allocator->Alloc(nbytes, buffer_ptr, alloc_context);
                    if (buffer_ptr && alloc_context) {
                        InferenceEngine::Blob::Ptr blob;
                        switch (layer.second.getPrecision()) {
                        case InferenceEngine::Precision::U8:
                            blob = InferenceEngine::make_shared_blob<uint8_t>(layer.second, (uint8_t *)buffer_ptr);
                            break;
                        case InferenceEngine::Precision::FP32:
                            blob = InferenceEngine::make_shared_blob<float>(layer.second, (float *)buffer_ptr);
                            break;
                        default:
                            throw std::logic_error("Unsupported precision of the layer");
                        }
                        req->infer_request->SetBlob(layer.first, blob);
                        req->alloc_context.push_back(alloc_context);
                    }
                }
            }
            freeRequests.push(req);
        }
    }
    initialized = true;
    this->callback = callback;
}

bool OpenVINOImageInference::IsQueueFull() {
    return freeRequests.empty();
}

void OpenVINOImageInference::GetNextImageBuffer(std::shared_ptr<BatchRequest> request, Image *image) {
    GVA_DEBUG(__FUNCTION__);
    ITT_TASK(__FUNCTION__);

    if (inputs.begin() == inputs.end())
        throw std::runtime_error("Inputs data map is empty.");
    std::string inputName = inputs.begin()->first; // assuming one input layer
    auto blob = request->infer_request->GetBlob(inputName);
    auto blobSize = blob.get()->dims();

    *image = {};
    image->width = blobSize[0];
    image->height = blobSize[1];
    image->format = FOURCC_RGBP;
    int batchIndex = request->buffers.size();
    int plane_size = image->width * image->height;
    image->planes[0] = blob->buffer().as<uint8_t *>() + batchIndex * plane_size * blobSize[2];
    image->planes[1] = image->planes[0] + plane_size;
    image->planes[2] = image->planes[1] + plane_size;
    image->stride[0] = image->width;
    image->stride[1] = image->width;
    image->stride[2] = image->width;
}

void OpenVINOImageInference::SubmitImageSoftwarePreProcess(std::shared_ptr<BatchRequest> request, const Image &srcImg,
                                                           std::function<void(Image &)> preProcessor) {
    if (!sw_vpp.get()) {
        auto frameBlob = WrapImage2Blob(srcImg);
        std::string inputName = inputs.begin()->first;
        request->infer_request->SetBlob(inputName, frameBlob);
    } else {
        Image dstImg = {};
        GetNextImageBuffer(request, &dstImg);
        if (srcImg.planes[0] != dstImg.planes[0]) { // only convert if different buffers
            try {
                sw_vpp->Convert(srcImg, dstImg);
                // You can check preProcessor result with the following code snippet:
                // ----------------------------------------------
                // cv::Mat mat(dst.height, dst.width, CV_8UC1, dst.planes[1], dst.stride[1]);
                // static int counter;
                // cv::imwrite(std::string("") + std::to_string(counter) + "_pre" + ".png", mat);
                // preProcessor(dst);
                // cv::imwrite(std::string("") + std::to_string(counter++) + "_post" + ".png", mat);
                preProcessor(dstImg);
            } catch (const std::exception &e) {
                std::throw_with_nested(std::runtime_error("Error while preprocessing"));
            }
        }
    }
}

void OpenVINOImageInference::SubmitImage(const Image &image, IFramePtr user_data,
                                         std::function<void(Image &)> preProcessor) {
    GVA_DEBUG(__FUNCTION__);
    ITT_TASK(__FUNCTION__);

    ++requests_processing_;
    auto request = freeRequests.pop();

    SubmitImageSoftwarePreProcess(request, image, preProcessor);

    request->buffers.push_back(user_data);
    // start inference asynchronously if enough buffers for batching
    if (request->buffers.size() >= (size_t)batch_size) {
#if 1 // TODO: remove when license-plate-recognition-barrier model will take one input
        if (inputs.count("seq_ind")) {
            // 'seq_ind' input layer is some relic from the training
            // it should have the leading 0.0f and rest 1.0f
            IE::Blob::Ptr seqBlob = request->infer_request->GetBlob("seq_ind");
            int maxSequenceSizePerPlate = seqBlob->getTensorDesc().getDims()[0];
            float *blob_data = seqBlob->buffer().as<float *>();
            blob_data[0] = 0.0f;
            std::fill(blob_data + 1, blob_data + maxSequenceSizePerPlate, 1.0f);
        }
#endif
        request->infer_request->StartAsync();
    } else {
        freeRequests.push_front(request);
    }
}

const std::string &OpenVINOImageInference::GetModelName() const {
    return modelName;
}

void OpenVINOImageInference::GetModelInputInfo(int *width, int *height, int *format) const {
    auto dims = inputs.begin()->second->getDims();
    if (dims.size() < 2)
        throw std::runtime_error("Incorrect model input dimensions");
    *width = dims[0];
    *height = dims[1];
    *format = FOURCC_RGBP;
}

void OpenVINOImageInference::Flush() {
    std::unique_lock<std::mutex> lk(mutex_);
    if (requests_processing_ != 0) {
        request_processed_.wait_for(lk, std::chrono::seconds(1), [this] { return requests_processing_ == 0; });
    }
}

void OpenVINOImageInference::Close() {
    Flush();
    while (!freeRequests.empty()) {
        auto req = freeRequests.pop();
        // as earlier set callbacks own shared pointers we need to set lambdas with the empty capture lists
        req->infer_request->SetCompletionCallback([] {});
        if (allocator) {
            for (auto ac : req->alloc_context)
                allocator->Free(ac);
        }
    }
}

void OpenVINOImageInference::WorkingFunction(std::shared_ptr<BatchRequest> request) {
    GVA_DEBUG(__FUNCTION__);
    std::map<std::string, OutputBlob::Ptr> output_blobs;
    for (auto output : outputs) {
        const std::string &name = output.first;
        output_blobs[name] = std::make_shared<IEOutputBlob>(request->infer_request->GetBlob(name));
    }
    callback(output_blobs, request->buffers);
    request->buffers.clear();
    freeRequests.push(request);
}
