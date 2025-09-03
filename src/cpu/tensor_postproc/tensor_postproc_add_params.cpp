/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/base/transform.h"

#include "dlstreamer/cpu/elements/tensor_postproc_add_params.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/utils.h"

namespace dlstreamer {

namespace param {
static constexpr auto attribute_name = "attribute-name";
static constexpr auto format = "format";
}; // namespace param

static ParamDescVector params_desc = {
    {param::attribute_name, "Name for metadata created and attached by this element", "attribute"},
    {param::format, "Format description", std::string()},
};

class PostProcAddParams : public BaseTransformInplace {
  public:
    PostProcAddParams(DictionaryCPtr params, const ContextPtr &app_context)
        : BaseTransformInplace(app_context), _params(params) {
        _attribute_name = params->get(param::attribute_name, std::string("attribute"));
    }

    bool process(FramePtr frame) override {
        auto meta = frame->metadata().add(_attribute_name);
        copy_dictionary(*_params, *meta, true);
        return true;
    }

  private:
    std::string _attribute_name;
    DictionaryCPtr _params;
};

extern "C" {
ElementDesc tensor_postproc_add_params = {.name = "tensor_postproc_add_params",
                                          .description =
                                              "Post-processing to only add properties/parameters to metadata",
                                          .author = "Intel Corporation",
                                          .params = &params_desc,
                                          .input_info = MAKE_FRAME_INFO_VECTOR({MediaType::Tensors}),
                                          .output_info = MAKE_FRAME_INFO_VECTOR({MediaType::Tensors}),
                                          .create = create_element<PostProcAddParams>,
                                          .flags = 0};
}

} // namespace dlstreamer
