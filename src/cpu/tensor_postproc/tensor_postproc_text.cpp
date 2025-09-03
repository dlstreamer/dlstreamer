/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/cpu/elements/tensor_postproc_text.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/utils.h"

#include <iomanip>
#include <limits>
#include <sstream>

namespace dlstreamer {

namespace param {
static constexpr auto text_scale = "text-scale";         // (double)
static constexpr auto text_precision = "text-precision"; // (int)
static constexpr auto attribute_name = "attribute-name";
static constexpr auto layer_name = "layer-name"; // (string)

static constexpr double default_text_scale = 1.0;
static constexpr int default_text_precision = 0;
}; // namespace param

static ParamDescVector params_desc = {
    {param::text_scale, "Scale tensor values before converting to text", param::default_text_scale, 0.0,
     std::numeric_limits<double>::max()},
    {param::text_precision, "Precision for floating-point to text conversion", param::default_text_precision, (int)0,
     std::numeric_limits<int>::max()},
    {param::attribute_name, "Name for metadata created and attached by this element", std::string()},
    {param::layer_name, "Name of output layer to process (in case of multiple output tensors)", std::string()},
};

class PostProcText : public BaseTransformInplace {
  public:
    PostProcText(DictionaryCPtr params, const ContextPtr &app_context)
        : BaseTransformInplace(app_context), _params(params) {
        _scale = params->get<double>(param::text_scale, param::default_text_scale);
        _precision = params->get<int>(param::text_precision, param::default_text_precision);
        _attribute_name = params->get(param::attribute_name, std::string());
        _layer_name = params->get<std::string>(param::layer_name);
    }

    bool process(FramePtr frame) override {
        auto src = frame.map(AccessMode::Read);
        auto model_info = find_metadata<ModelInfoMetadata>(*frame);

        TensorPtr tensor_to_process;
        if (!_layer_name.empty()) {
            if (!model_info)
                throw std::runtime_error("Layer name specified but model info not found");

            auto lnames = model_info->output_layers();
            auto it = std::find(lnames.cbegin(), lnames.cend(), _layer_name);
            if (it == lnames.end())
                throw std::runtime_error("There's no output layer with name:" + _layer_name);
            tensor_to_process = src->tensor(std::distance(lnames.cbegin(), it));
        }

        auto meta = add_metadata<ClassificationMetadata>(*frame);
        copy_dictionary(*_params, meta, true);
        if (!_attribute_name.empty())
            meta.set_name(_attribute_name);
        if (model_info)
            meta.set_model_name(model_info->model_name());

        // convert tensor data to string
        std::stringstream stream;
        stream << std::fixed << std::setprecision(_precision);

        if (tensor_to_process) {
            // Convert tensor for specified layer name
            process_tensor(*tensor_to_process, stream);
        } else {
            // Conver all possible tensors
            for (auto tensor : src) {
                process_tensor(*tensor, stream);
            }
        }

        meta.set_label(stream.str());
        return true;
    }

    void process_tensor(const Tensor &tensor, std::stringstream &out) {
        auto &info = tensor.info();
        if (info.dtype != DataType::Float32)
            throw std::runtime_error("Only DataType::Float32 supported");
        if (!info.is_contiguous())
            throw std::runtime_error("Contiguous frame expected");
        const float *data = tensor.data<float>();
        for (size_t i = 0; i < info.size(); i++) {
            if (i)
                out << ", ";
            out << data[i] * _scale;
        }
    }

  private:
    DictionaryCPtr _params;
    double _scale = param::default_text_scale;
    int _precision = param::default_text_precision;
    std::string _attribute_name;
    std::string _layer_name;
};

extern "C" {
ElementDesc tensor_postproc_text = {.name = "tensor_postproc_text",
                                    .description = "Post-processing to convert tensor data into text",
                                    .author = "Intel Corporation",
                                    .params = &params_desc,
                                    .input_info = MAKE_FRAME_INFO_VECTOR({MediaType::Tensors}),
                                    .output_info = MAKE_FRAME_INFO_VECTOR({MediaType::Tensors}),
                                    .create = create_element<PostProcText>,
                                    .flags = 0};
}

} // namespace dlstreamer
