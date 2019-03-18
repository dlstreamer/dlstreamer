/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
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

// For debuging purposes uncomment the following to lines
// #include <opencv2/opencv.hpp>
// #include <string>

namespace IE = InferenceEngine;

inline std::string fileNameNoExt(const std::string &filepath) {
    auto pos = filepath.rfind('.');
    if (pos == std::string::npos)
        return filepath;
    return filepath.substr(0, pos);
}

inline int getNumberChannels(int format) {
    switch (format) {
    case InferenceBackend::FOURCC_BGRA:
        return 4;
    case InferenceBackend::FOURCC_BGRX:
        return 4;
    case InferenceBackend::FOURCC_BGR:
        return 3;
    case InferenceBackend::FOURCC_RGBA:
        return 4;
    case InferenceBackend::FOURCC_RGBX:
        return 4;
    }
    return 0;
}

inline InferenceEngine::Blob::Ptr wrapMat2Blob(const Image &image) {
    GVA_DEBUG(__FUNCTION__);
    ITT_TASK(__FUNCTION__);

    int channels = getNumberChannels(image.format);
    if (image.stride[0] != channels * image.width)
        throw std::runtime_error("Doesn't support conversion from not dense image");

    InferenceEngine::TensorDesc tDesc(InferenceEngine::Precision::U8,
                                      {1, (size_t)channels, (size_t)image.height, (size_t)image.width},
                                      InferenceEngine::Layout::NHWC);

    return InferenceEngine::make_shared_blob<uint8_t>(tDesc, image.planes[0]);
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
                                               const std::map<std::string, std::string> &config, CallbackFunc callback)
    : batch_size(batch_size), already_flushed(false) {

    GVA_DEBUG("Image Inference construct")

    resize_by_inference =
        config.count(KEY_RESIZE_BY_INFERENCE) > 0 ? config.at(KEY_RESIZE_BY_INFERENCE) == "TRUE" : false;

    auto devices_vec = split(devices, '-');
    std::string cpu_extension = config.count(KEY_CPU_EXTENSION) ? config.at(KEY_CPU_EXTENSION) : "";
    // TODO: Inference Engine asserts if unknown key passed
    std::map<std::string, std::string> ie_config(config);
    ie_config.erase(KEY_RESIZE_BY_INFERENCE);
    ie_config.erase(KEY_CPU_EXTENSION);

    for (std::string device : devices_vec) { // '-' separated list of devices
        IE::InferencePlugin::Ptr plugin = PluginsHolderSingleton::getInstance().getPluginPtr(device);
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

        // Load network (.xml and .bin files)
        InferenceEngine::CNNNetReader netReader;
        netReader.ReadNetwork(model);
        netReader.ReadWeights(fileNameNoExt(model) + ".bin");
        modelName = netReader.getName();

        // Check model input
        IE::InputsDataMap inputInfo(netReader.getNetwork().getInputsInfo());
        if (inputInfo.size() == 0) { //!= 1) {
            throw std::logic_error("Input layer not found");
        }
        auto &input = inputInfo.begin()->second;
        input->setPrecision(IE::Precision::U8);
        if (resize_by_inference) {
            input->getPreProcess().setResizeAlgorithm(IE::ResizeAlgorithm::RESIZE_BILINEAR);
            input->getInputData()->setLayout(IE::Layout::NHWC);
        } else {
            input->getInputData()->setLayout(IE::Layout::NCHW);
        }

        netReader.getNetwork().setBatchSize(batch_size);

        // LoadNetwork
        auto network = plugin->LoadNetwork(netReader.getNetwork(), ie_config);

        inputs = network.GetInputsInfo();
        InferenceEngine::ConstOutputsDataMap outputs = network.GetOutputsInfo();
        for (auto output : outputs) {
            if (auto layer = output.second->creatorLayer.lock()) {
                layerNameToType.emplace(output.first, layer->type);
            }
        }

        for (int i = 0; i < nireq; i++) {
            BatchRequest req = {};
            req.infer_request = network.CreateInferRequestPtr();
            freeRequests.push(req);
        }
    }

    this->callback = callback;
    working_thread = std::thread(&OpenVINOImageInference::WorkingFunction, this);
}

bool OpenVINOImageInference::IsQueueFull() {
    return freeRequests.empty();
}

void OpenVINOImageInference::GetNextImageBuffer(BatchRequest &request, Image *image) {
    GVA_DEBUG(__FUNCTION__);
    ITT_TASK(__FUNCTION__);

    std::string inputName = inputs.begin()->first; // assuming one input layer
    auto blob = request.infer_request->GetBlob(inputName);
    auto blobSize = blob.get()->dims();

    *image = {};
    image->width = blobSize[0];
    image->height = blobSize[1];
    image->format = FOURCC_RGBP;
    int batchIndex = request.buffers.size();
    int plane_size = image->width * image->height;
    image->planes[0] = blob->buffer().as<uint8_t *>() + batchIndex * plane_size * blobSize[2];
    image->planes[1] = image->planes[0] + plane_size;
    image->planes[2] = image->planes[1] + plane_size;
    image->stride[0] = image->width;
    image->stride[1] = image->width;
    image->stride[2] = image->width;
}

__inline Image ApplyCrop(const Image &src) {
    GVA_DEBUG(__FUNCTION__);
    Image dst = src;
    dst.rect = {};

    int planes_count = GetPlanesCount(src.format);
    if (!src.rect.width && !src.rect.height) {
        dst = src;
        for (int i = 0; i < planes_count; i++)
            dst.planes[i] = src.planes[i];
        return dst;
    }

    if (src.rect.x >= src.width || src.rect.y >= src.height || src.rect.x + src.rect.width <= 0 ||
        src.rect.y + src.rect.height <= 0) {
        throw std::runtime_error("ERROR: ApplyCrop: Requested rectangle is out of image boundaries\n");
    }

    int rect_x = std::max(src.rect.x, 0);
    int rect_y = std::max(src.rect.y, 0);
    int rect_width = std::min(src.rect.width - (rect_x - src.rect.x), src.width - rect_x);
    int rect_height = std::min(src.rect.height - (rect_y - src.rect.y), src.height - rect_y);

    switch (src.format) {
    case InferenceBackend::FOURCC_NV12: {
        dst.planes[0] = src.planes[0] + rect_y * src.stride[0] + rect_x;
        dst.planes[1] = src.planes[1] + (rect_y / 2) * src.stride[1] + rect_x;
        break;
    }
    case InferenceBackend::FOURCC_I420: {
        dst.planes[0] = src.planes[0] + rect_y * src.stride[0] + rect_x;
        dst.planes[1] = src.planes[1] + (rect_y / 2) * src.stride[1] + (rect_x / 2);
        dst.planes[2] = src.planes[2] + (rect_y / 2) * src.stride[2] + (rect_x / 2);
        break;
    }
    case InferenceBackend::FOURCC_RGBP: {
        dst.planes[0] = src.planes[0] + rect_y * src.stride[0] + rect_x;
        dst.planes[1] = src.planes[1] + rect_y * src.stride[1] + rect_x;
        dst.planes[2] = src.planes[2] + rect_y * src.stride[2] + rect_x;
        break;
    }
    case InferenceBackend::FOURCC_BGR: {
        int channels = 3;
        dst.planes[0] = src.planes[0] + rect_y * src.stride[0] + rect_x * channels;
        break;
    }
    default: {
        int channels = 4;
        dst.planes[0] = src.planes[0] + rect_y * src.stride[0] + rect_x * channels;
        break;
    }
    }

    if (rect_width)
        dst.width = rect_width;
    if (rect_height)
        dst.height = rect_height;

    return dst;
}

void OpenVINOImageInference::SubmitImage(const Image &image, IFramePtr user_data,
                                         std::function<void(Image &)> preProcessor) {
    GVA_DEBUG(__FUNCTION__);
    ITT_TASK(__FUNCTION__);
    const Image *pSrc = &image;
    Image resized = {};

    if (image.type == MemoryType::VAAPI) {
        if (!vaapi_vpp.get())
            vaapi_vpp.reset(PreProc::Create(MemoryType::VAAPI));

        auto model_input_size = inputs.begin()->second->getDims();
        resized.format = InferenceBackend::FOURCC_RGBP;
        resized.width = (int)model_input_size[0];
        resized.height = (int)model_input_size[1];
        vaapi_vpp->Convert(image, resized, true);
        pSrc = &resized;
    }

    // front() call blocks if freeRequests is empty, i.e all requests still in workingRequests list and not completed
    auto request = freeRequests.pop();

    if (resize_by_inference) {
        auto frameBlob = wrapMat2Blob(*pSrc);
        std::string inputName = inputs.begin()->first; // assuming one input layer
        request.infer_request->SetBlob(inputName, frameBlob);
    } else {
        Image dst = {};
        GetNextImageBuffer(request, &dst);

        if (pSrc->planes[0] != dst.planes[0]) { // only convert if different buffers
            Image src = ApplyCrop(*pSrc);
            if (!sw_vpp.get()) {
                sw_vpp.reset(PreProc::Create(MemoryType::SYSTEM));
            }
            sw_vpp->Convert(src, dst);

            // You can check preProcessor result with the following code snippet:
            // ----------------------------------------------
            // cv::Mat mat(dst.height, dst.width, CV_8UC1, dst.planes[1], dst.stride[1]);
            // static int counter;
            // cv::imwrite(std::string("") + std::to_string(counter) + "_pre" + ".png", mat);
            // preProcessor(dst);
            // cv::imwrite(std::string("") + std::to_string(counter++) + "_post" + ".png", mat);
            preProcessor(dst);
        }
    }

    if (pSrc == &resized) {
        vaapi_vpp->ReleaseImage(resized);
    }

    request.buffers.push_back(user_data);

    // start inference asynchronously if enough buffers for batching
    if (request.buffers.size() >= (size_t)batch_size) {
#if 1 // TODO: remove when license-plate-recognition-barrier model will take one input
        if (inputs.count("seq_ind")) {
            // 'seq_ind' input layer is some relic from the training
            // it should have the leading 0.0f and rest 1.0f
            IE::Blob::Ptr seqBlob = request.infer_request->GetBlob("seq_ind");
            int maxSequenceSizePerPlate = seqBlob->getTensorDesc().getDims()[0];
            float *blob_data = seqBlob->buffer().as<float *>();
            blob_data[0] = 0.0f;
            std::fill(blob_data + 1, blob_data + maxSequenceSizePerPlate, 1.0f);
        }
#endif
        request.infer_request->StartAsync();
        workingRequests.push(request);
    } else {
        freeRequests.push_front(request);
    }
}

const std::string &OpenVINOImageInference::GetLayerTypeByLayerName(const std::string &layer_name) const {
    static const std::string default_value = "";
    auto it = layerNameToType.find(layer_name);
    if (it != layerNameToType.end())
        return it->second;
    else
        return default_value;
}

const std::string &OpenVINOImageInference::GetModelName() const {
    return modelName;
}

void OpenVINOImageInference::Flush() {
    std::lock_guard<std::mutex> lock(flush_mutex);
    if (already_flushed) {
        return;
    }

    if (!working_thread.joinable())
        return;

    already_flushed = true;
    auto request = freeRequests.pop();
    if (!request.buffers.empty()) {
        request.infer_request->StartAsync();
        workingRequests.push(request);
    }
    workingRequests.waitEmpty();
}

void OpenVINOImageInference::Close() {
    Flush();
    if (working_thread.joinable()) {
        // add empty request
        BatchRequest req = {};
        workingRequests.push(req);
        // wait for thread reaching empty request
        working_thread.join();
    }
}

void OpenVINOImageInference::WorkingFunction() {
    GVA_DEBUG(__FUNCTION__);
    for (;;) {
        ITT_TASK(__FUNCTION__);
        auto request = workingRequests.front();
        if (request.buffers.empty()) {
            break;
        }
        IE::StatusCode sts;
        {
            ITT_TASK("Wait Inference");
            sts = request.infer_request->Wait(IE::IInferRequest::WaitMode::RESULT_READY);
        }
        if (IE::OK == sts) {
            try {
                std::map<std::string, OutputBlob::Ptr> output_blobs;
                for (const std::pair<std::string, std::string> &nameToType : layerNameToType) {
                    const std::string &name = nameToType.first;
                    output_blobs[name] = std::make_shared<IEOutputBlob>(request.infer_request->GetBlob(name));
                }
                callback(output_blobs, request.buffers);
            } catch (const std::exception &exc) {
#ifdef DEBUG
                printf("Exception in inference callback: %s", exc.what());
#endif
            }
        } else {
#ifdef DEBUG
            printf("Inference Error: %d", sts);
#endif
        }

        // move request from workingRequests to freeRequests list
        request = workingRequests.pop();
        request.buffers.clear();
        freeRequests.push(request);
    }
}

ImageInference::Ptr ImageInference::make_shared(MemoryType, std::string devices, std::string model, int batch_size,
                                                int nireq, const std::map<std::string, std::string> &config,
                                                CallbackFunc callback) {
    return std::make_shared<OpenVINOImageInference>(devices, model, batch_size, nireq, config, callback);
}
