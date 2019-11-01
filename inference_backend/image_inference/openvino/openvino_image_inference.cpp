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

#ifdef ENABLE_CPU_EXTENSIONS
#include <ext_list.hpp>
#endif

#include <chrono>
#include <ie_compound_blob.h>

// For debuging purposes uncomment the following to lines
// #include <opencv2/opencv.hpp>
// #include <string>

namespace IE = InferenceEngine;
using namespace InferenceBackend;

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
        const size_t imageWidth = (size_t)image.width;
        const size_t imageHeight = (size_t)image.height;
        IE::TensorDesc planeY(IE::Precision::U8, {1, 1, imageHeight, imageWidth}, IE::Layout::NHWC);
        IE::TensorDesc planeUV(IE::Precision::U8, {1, 2, imageHeight / 2, imageWidth / 2}, IE::Layout::NHWC);

        IE::ROI crop_roi_y({
            0,
            (size_t)((image.rect.x & 0x1) ? image.rect.x - 1 : image.rect.x),
            (size_t)((image.rect.y & 0x1) ? image.rect.y - 1 : image.rect.y),
            (size_t)((image.rect.width & 0x1) ? image.rect.width - 1 : image.rect.width),
            (size_t)((image.rect.height & 0x1) ? image.rect.height - 1 : image.rect.height),
        });

        IE::ROI crop_roi_uv({0, (size_t)image.rect.x / 2, (size_t)image.rect.y / 2, (size_t)image.rect.width / 2,
                             (size_t)image.rect.height / 2});

        IE::Blob::Ptr blobY = IE::make_shared_blob<uint8_t>(planeY, image.planes[0]);
        IE::Blob::Ptr blobUV = IE::make_shared_blob<uint8_t>(planeUV, image.planes[1]);

        IE::Blob::Ptr y_plane_with_roi = IE::make_shared_blob(blobY, crop_roi_y);
        IE::Blob::Ptr uv_plane_with_roi = IE::make_shared_blob(blobUV, crop_roi_uv);

        IE::Blob::Ptr nv12Blob = IE::make_shared_blob<IE::NV12Blob>(y_plane_with_roi, uv_plane_with_roi);
        return nv12Blob;
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

    : allocator(allocator), batch_size(batch_size), requests_processing_(0U) {

    GVA_DEBUG("Image Inference construct");

    auto devices_vec = split(devices, '-');
    const std::string &cpu_extension = config.count(KEY_CPU_EXTENSION) ? config.at(KEY_CPU_EXTENSION) : "";
    const std::string &preProcessorName = config.count(KEY_PRE_PROCESSOR_TYPE) ? config.at(KEY_PRE_PROCESSOR_TYPE) : "";
    const std::string &imageFormatName = config.count(KEY_IMAGE_FORMAT) ? config.at(KEY_IMAGE_FORMAT) : "";
    // TODO: Inference Engine asserts if unknown key passed
    std::map<std::string, std::string> ie_config(config);
    ie_config.erase(KEY_CPU_EXTENSION);
    ie_config.erase(KEY_PRE_PROCESSOR_TYPE);
    ie_config.erase(KEY_IMAGE_FORMAT);

    std::function<void(IE::InputsDataMap &)> initPreprocessor =
        [this, preProcessorName, imageFormatName](IE::InputsDataMap &model_inputs_info) {
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
                if (preProcessorName.empty() || preProcessorName == "opencv")
                    preProcessorType = PreProcessType::OpenCV;
                else if (preProcessorName == "g-api")
                    preProcessorType = PreProcessType::GAPI;
                else if (preProcessorName == "vaapi")
                    preProcessorType = PreProcessType::VAAPI;
                try {
                    this->sw_vpp.reset(PreProc::Create(preProcessorType));
                } catch (const std::exception &e) {
                    std::rethrow_if_nested(e);
                }

                if (!this->sw_vpp.get()) {
                    // TODO ERROR
                }
            }
        };

    bool is_ir_model = true;
    InferenceEngine::CNNNetwork network;
    std::string model_bin = "";
    try {
        // Load IR network (.xml and .bin files)
        InferenceEngine::CNNNetReader network_reader;
        network_reader.ReadNetwork(model);
        model_bin = fileNameNoExt(model) + ".bin";
        network_reader.ReadWeights(model_bin);
        modelName = network_reader.getName();
        network = network_reader.getNetwork();

        // Check model input
        IE::InputsDataMap model_inputs_info(network.getInputsInfo());
        initPreprocessor(model_inputs_info);
        network.setBatchSize(batch_size);
    } catch (InferenceEngine::details::InferenceEngineException &e) {
        GVA_WARNING(("Could not read IR model from files: " + model + ", " + model_bin +
                     ". Model will be treated as pre-compiled.")
                        .c_str());
        is_ir_model = false;
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Error during loading IR model from " + model));
    }

    for (const std::string &device : devices_vec) { // '-' separated list of devices
        IE::InferencePlugin::Ptr plugin;
        try {
            plugin = PluginsHolderSingleton::getInstance().getPluginPtr(device);
        } catch (const std::exception &e) {
            std::throw_with_nested(std::runtime_error("Could not create plugin for device: " + device));
        }

        // Extension for custom layers
#ifdef ENABLE_CPU_EXTENSIONS
        if (device.find("CPU") != std::string::npos) {
            // library with custom layers
            plugin->AddExtension(std::make_shared<IE::Extensions::Cpu::CpuExtensions>());

            if (!cpu_extension.empty()) {
                // CPU(MKLDNN) extensions are loaded as a shared library and passed as a pointer to base extension
                auto extension_ptr = IE::make_so_pointer<IE::IExtension>(cpu_extension);
                plugin->AddExtension(extension_ptr);
            }
        }
#endif
        plugins.push_back(plugin);

        InferenceEngine::ExecutableNetwork executable_network;
        if (is_ir_model) {
            // Loading IR model to the device
            executable_network = plugin->LoadNetwork(network, ie_config);
        } else {
            try {
                // Importing pre-compiled model to the device
                executable_network = plugin->ImportNetwork(model, ie_config);
            } catch (const std::exception &e) {
                std::throw_with_nested(std::runtime_error("Couldn't read network/import pre-compiled model '" + model +
                                                          "' for device: " + device));
            }

            // Check model input
            IE::ConstInputsDataMap const_model_inputs_info(executable_network.GetInputsInfo());
            IE::InputsDataMap model_inputs_info;

            // Workaround IE API to fill model_inputs_info with mutable pointers
            std::for_each(const_model_inputs_info.begin(), const_model_inputs_info.end(),
                          [&](std::pair<const std::string, IE::InputInfo::CPtr> pair) {
                              model_inputs_info.emplace(pair.first,
                                                        std::const_pointer_cast<IE::InputInfo>(pair.second));
                          });

            initPreprocessor(model_inputs_info);
        }

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

Image OpenVINOImageInference::GetNextImageBuffer(std::shared_ptr<BatchRequest> request) {
    GVA_DEBUG(__FUNCTION__);
    ITT_TASK(__FUNCTION__);

    if (inputs.begin() == inputs.end())
        throw std::runtime_error("Inputs data map is empty.");
    std::string inputName = inputs.begin()->first; // assuming one input layer
    auto blob = request->infer_request->GetBlob(inputName);
    auto dims = blob->getTensorDesc().getDims();
    std::reverse(dims.begin(), dims.end());

    Image image = Image();
    image.width = dims[0];
    image.height = dims[1];
    image.format = FOURCC_RGBP;
    int batchIndex = request->buffers.size();
    int plane_size = image.width * image.height;
    image.planes[0] = blob->buffer().as<uint8_t *>() + batchIndex * plane_size * dims[2];
    image.planes[1] = image.planes[0] + plane_size;
    image.planes[2] = image.planes[1] + plane_size;
    image.stride[0] = image.width;
    image.stride[1] = image.width;
    image.stride[2] = image.width;
    return image;
}

void OpenVINOImageInference::SubmitImageSoftwarePreProcess(std::shared_ptr<BatchRequest> request, const Image &srcImg,
                                                           std::function<void(Image &)> preProcessor) {
    if (!sw_vpp.get()) {
        auto frameBlob = WrapImage2Blob(srcImg);
        std::string inputName = inputs.begin()->first;
        request->infer_request->SetBlob(inputName, frameBlob);
    } else {
        Image dstImg = GetNextImageBuffer(request);
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
    auto dims = inputs.begin()->second->getTensorDesc().getDims();
    std::reverse(dims.begin(), dims.end());
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
