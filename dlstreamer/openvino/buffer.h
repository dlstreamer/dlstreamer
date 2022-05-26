/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/buffer_base.h"
#include "dlstreamer/openvino/utils.h"

namespace dlstreamer {

class OpenVINOBuffer : public BufferBase {
  protected:
    // Protected ctor for inheritance purpose
    OpenVINOBuffer(BufferInfoCPtr buffer_info) : BufferBase(BufferType::OPENVINO, std::move(buffer_info)) {
    }

  public:
    /**
     * @brief If an inference request is associated with the buffer, waits for inference to complete, otherwise returns
     * immediatly.
     */
    virtual void wait() = 0;

    /**
     * @brief Returns raw data pointer for tensor at specified index
     *
     * @param index index of tensor
     * @return void* pointer to data
     */
    virtual void *data(size_t index) = 0;
};

using OpenVinoBufferPtr = std::shared_ptr<OpenVINOBuffer>;

// FIXME: simplify two classes below into single template class?

// Buffer based on OpenVINO™ toolkit 1.0 API with tensor represented as IE::Blob type
class OpenVINOBlobsBuffer : public OpenVINOBuffer {
  public:
    using Blobs = std::vector<IE::Blob::Ptr>;

    OpenVINOBlobsBuffer(const Blobs &blobs, IE::InferRequest::Ptr infer_request = nullptr)
        : OpenVINOBuffer(blobs_to_buffer_info(blobs)), _blobs(blobs), _infer_request(infer_request) {
    }

    IE::Blob::Ptr blob(int index) {
        return _blobs[index];
    }

    IE::InferRequest::Ptr infer_request() {
        return _infer_request;
    }

    void capture_input(BufferPtr input_buffer) {
        _input_buffer = input_buffer;
    }

    // OpenVINOBuffer overrides

    void wait() override {
        if (_infer_request) {
            _infer_request->Wait();
            // After inference is completed the input buffer can be released
            _input_buffer.reset();
        }
    }

    void *data(size_t index) override {
        return _blobs.at(index)->buffer().as<void *>();
    }

  protected:
    Blobs _blobs;
    IE::InferRequest::Ptr _infer_request;
    BufferPtr _input_buffer;

    BufferInfoCPtr blobs_to_buffer_info(const Blobs &blobs) {
        auto info = std::make_shared<BufferInfo>();
        for (const auto &blob : blobs) {
            info->planes.push_back(tensor_desc_to_plane_info(blob->getTensorDesc()));
        }
        return info;
    }
};

using OpenVINOBlobsBufferPtr = std::shared_ptr<OpenVINOBlobsBuffer>;

#ifdef HAVE_OPENVINO2
// Buffer based on OpenVINO™ toolkit 2.0 API with tensor represented as ov::runtime::Tensor type.
class OpenVinoTensorsBuffer : public OpenVINOBuffer {
  public:
    OpenVinoTensorsBuffer(ov::runtime::TensorVector &&tensors, const std::vector<std::string> &names,
                          ov::InferRequest infer_request)
        : OpenVINOBuffer(tensors_to_buffer_info(tensors, names)), _tensors(std::move(tensors)),
          _infer_req(infer_request) {
    }

    OpenVinoTensorsBuffer(ov::runtime::TensorVector &&tensors, ov::InferRequest infer_request)
        : OpenVinoTensorsBuffer(std::move(tensors), {}, infer_request) {
    }

    ~OpenVinoTensorsBuffer() {
        if (_infer_req)
            _infer_req.wait();
    }

    const ov::runtime::TensorVector &tensors() const {
        return _tensors;
    }

    ov::InferRequest infer_request() {
        return _infer_req;
    }

    void capture_input(BufferPtr input_buffer) {
        _input_buffer = input_buffer;
    }

    // OpenVINOBuffer overrides

    void wait() override {
        if (_infer_req) {
            _infer_req.wait();
            // After inference is completed the input buffer can be released
            _input_buffer.reset();
        }
    }

    void *data(size_t index) override {
        return _tensors.at(index).data();
    }

  protected:
    static BufferInfoCPtr tensors_to_buffer_info(const ov::runtime::TensorVector &tensors,
                                                 const std::vector<std::string> &names) {
        if (!names.empty() && names.size() != tensors.size())
            throw std::runtime_error("Invalid size of names vector: the size must be equal to size of tensor vector");

        auto bi = std::make_shared<BufferInfo>();
        for (size_t i = 0; i < tensors.size(); i++) {
            bi->planes.emplace_back(tensors[i].get_shape(), data_type_from_openvino(tensors[i].get_element_type()));
            if (!names.empty())
                bi->planes.back().name = names[i];
        }
        return bi;
    }

    ov::runtime::TensorVector _tensors;
    ov::InferRequest _infer_req;
    BufferPtr _input_buffer;
};

using OpenVinoTensorsBufferPtr = std::shared_ptr<OpenVinoTensorsBuffer>;
#endif

} // namespace dlstreamer
