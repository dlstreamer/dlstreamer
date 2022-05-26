/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_openvino2.h"

#include "dlstreamer/buffer_mappers/cpu_to_openvino.h"
#include "dlstreamer/buffer_mappers/mapper_chain.h"
#include "dlstreamer/buffer_mappers/openvino_to_cpu.h"
#include "dlstreamer/openvino/buffer.h"
#include <openvino/openvino.hpp>

namespace dlstreamer {

namespace param {
constexpr auto model_path = "model";
}

class InferenceOpenVino2 : public TransformWithAlloc {
  public:
    InferenceOpenVino2(ITransformController &transform_ctrl, DictionaryCPtr params)
        : TransformWithAlloc(transform_ctrl, std::move(params)) {
        std::cout << "InferenceOpenVino2()" << std::endl;
        read_model();
    }

    BufferInfoVector get_input_info(const BufferInfo & /*output_info*/) override {
        BufferInfo buf_info(MediaType::TENSORS);
        for (auto &&item : _model->get_parameters()) {
            // FIXME: datatype?
            buf_info.planes.emplace_back(item->get_shape(), DataType::U8, item->get_friendly_name());
        }

        auto buf_info_nhwc = buf_info_new_layout(buf_info, Layout::NHWC, 4);
        return {buf_info, buf_info_nhwc};
    }

    BufferInfoVector get_output_info(const BufferInfo & /*input_info*/) override {
        BufferInfo buf_info(MediaType::TENSORS);
        for (auto &&item : _model->get_results()) {
            buf_info.planes.emplace_back(ov_node_to_plane_info(*item));
            buf_info.buffer_type = BufferType::OPENVINO;
        }
        return {buf_info};
    }

    void set_info(const BufferInfo &in_info, const BufferInfo &out_info) override {
        if (!is_in_out_valid(in_info, out_info))
            throw std::runtime_error("Input or output is no valid");
        _in_info = in_info;
        init_input_mapper();

        std::cout << "Loading model to device '" << _model->get_friendly_name() << "' ... " << std::flush;
        // Pre-processing
        ov::preprocess::PrePostProcessor ppp(_model);
        ov::Layout in_layout(_in_info.planes.front().layout.to_string());

        // set input tensor
        auto &input_info = ppp.input();
        input_info.tensor()
            .set_element_type(ov::element::u8)
            .set_layout(in_layout)
            .set_color_format(ov::preprocess::ColorFormat::RGBX);
        input_info.preprocess().convert_color(ov::preprocess::ColorFormat::RGB);
        input_info.model().set_layout("NCHW");

        // Add pre-processing steps to model
        _model = ppp.build();

        _executable_network = _core.compile_model(_model, "CPU");
        std::cout << "[OK]" << std::endl;
        std::cout << "Input shape: " << _model->input().get_shape() << std::endl;

        for (auto &&out : _model->outputs())
            _output_names.emplace_back(out.get_any_name());
    }

    ContextPtr get_context(const std::string &name) override {
        std::cout << "InferenceOpenVino2::get_context(" << name << ") -> not implemented" << std::endl;
        return nullptr;
    }

    //
    // Transform overrides
    //

    std::function<BufferPtr()> get_output_allocator() override {
        return [this] {
            auto req = _executable_network.create_infer_request();
            ov::runtime::TensorVector tensors;
            tensors.reserve(_output_names.size());

            for (auto &&out_name : _output_names) {
                tensors.emplace_back(req.get_tensor(out_name));
            }
            return std::make_shared<OpenVinoTensorsBuffer>(std::move(tensors), _output_names, req);
        };
    }

    bool process(BufferPtr src, BufferPtr dst) override {
        auto src_ov = _in_mapper->map<OpenVinoTensorsBuffer>(src, AccessMode::READ);
        auto dst_openvino = std::dynamic_pointer_cast<OpenVinoTensorsBuffer>(dst);
        auto infer_request = dst_openvino->infer_request();
        infer_request.wait();
        dst_openvino->capture_input(src);
        infer_request.set_input_tensors(src_ov->tensors());
        infer_request.start_async();
        return true;
    }

    BufferMapperPtr get_output_mapper() override {
        return std::make_shared<BufferMapperOpenVINOToCPU>();
    }

  private:
    static PlaneInfo ov_node_to_plane_info(const ov::Node &node) {
        const DataType dtype = data_type_from_openvino(node.get_element_type());
        return {node.get_shape(), dtype, node.get_friendly_name()};
    }

    static std::vector<size_t> change_shape_layout(const std::vector<size_t> &shape, Layout lcur, Layout lnew) {
        if (lcur == lnew)
            return shape;

        const int indices_cur[]{lcur.n_position(), lcur.h_position(), lcur.w_position(), lcur.c_position()};
        const int indices_new[]{lnew.n_position(), lnew.h_position(), lnew.w_position(), lnew.c_position()};

        std::vector<size_t> result(4);
        int layout_size = 0;
        for (size_t i = 0; i < std::size(indices_new); i++) {
            auto icur = indices_cur[i], inew = indices_new[i];
            // FIXME: magic numbers
            if (inew == -1)
                continue;
            if (icur == -1)
                result[inew] = 1;
            else
                result[inew] = shape[icur];
            layout_size++;
        }
        result.resize(layout_size);
        return result;
    }

    static BufferInfo buf_info_new_layout(const BufferInfo &base, Layout layout, int num_ch) {
        BufferInfo res = base;
        for (auto &&p : res.planes) {
            auto shape = change_shape_layout(p.shape, p.layout, layout);
            // FIXME: magic numbers
            if (layout.c_position() != -1)
                shape.at(layout.c_position()) = num_ch;

            p = PlaneInfo(shape, p.type, p.name);
        }
        return res;
    }

    void read_model() {
        auto path = _params->get<std::string>(param::model_path);
        _model = _core.read_model(path);
        print_model_info();
    }

    void init_input_mapper() {
        // FIXME: support OCL, VAAPI
        if (_in_info.buffer_type != BufferType::CPU)
            throw std::runtime_error("unsupported input memory type");

        _in_mapper = std::make_shared<BufferMapperChain>(BufferMapperChain{
            _transform_ctrl->create_input_mapper(BufferType::CPU), std::make_shared<BufferMapperCPUToOpenVINO2>()});
    }

    bool is_in_out_valid(const BufferInfo &input_info, const BufferInfo &output_info) {
        // Verify supported output
        bool valid_out =
            output_info.media_type == MediaType::TENSORS && output_info.buffer_type == BufferType::OPENVINO;
        if (!valid_out)
            return false;

        // FIXME: support multiple input layers
        if (input_info.planes.size() > 1) {
            std::cout << "Multiple inputs are not supported" << std::endl;
            return false;
        }

        const PlaneInfo &plane = input_info.planes.front();
        if (plane.layout != Layout::NCHW && plane.layout != Layout::NHWC) {
            std::cout << "Unsupported input layout: " << plane.layout.to_string() << std::endl;
            return false;
        }
        return true;
    }

    void print_model_info() {
        assert(_model);
        std::cout << "Model name: " << _model->get_friendly_name() << std::endl;
        for (auto &&param : _model->get_parameters()) {
            std::cout << " [in ] " << param->get_friendly_name() << " : " << param->get_element_type() << " | "
                      << param->get_layout().to_string() << " | " << param->get_shape() << std::endl;
        }

        for (auto &&res : _model->get_results()) {
            std::cout << " [out] " << res->get_friendly_name() << " : " << res->get_element_type() << " | "
                      << res->get_layout().to_string() << " | " << res->get_shape() << std::endl;
        }
    }

  private:
    ov::Core _core;
    std::shared_ptr<ov::Model> _model;
    ov::CompiledModel _executable_network;

    BufferInfo _in_info;
    BufferMapperPtr _in_mapper;
    std::vector<std::string> _output_names;
};

static ParamDescVector inputParams = {
    {param::model_path, "Path to model file in OpenVINO™ toolkit or ONNX format", ""}};

TransformDesc TensorInferenceOpenVINO_2_0_Desc = {.name = "tensor_inference_openvino2",
                                                  .description = "Inference on OpenVINO™ toolkit backend using 2.0 API",
                                                  .author = "Intel Corporation",
                                                  .params = &inputParams,
                                                  .input_info = {{MediaType::TENSORS, BufferType::CPU}},
                                                  .output_info = {{MediaType::TENSORS, BufferType::OPENVINO}},
                                                  .create = TransformBase::create<InferenceOpenVino2>};

} // namespace dlstreamer
