/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "post_processor/blob_to_meta_converter.h"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace post_processing {

class ToLabelConverter : public BlobToTensorConverter {
    bool bMax = false;
    bool bCompound = false;
    bool bIndex = false;

  public:
    ToLabelConverter(const std::string &model_name, const ModelImageInputInfo &input_image_info,
                     GstStructureUniquePtr model_proc_output_info, const std::vector<std::string> &labels)
        : BlobToTensorConverter(model_name, input_image_info, std::move(model_proc_output_info), labels) {
        if (!raw_tesor_copying->enabled(RawTensorCopyingToggle::id))
            GVA_WARNING(RawTensorCopyingToggle::deprecation_message.c_str());

        GstStructure *s = getModelProcOutputInfo().get();
        const std::string method = gst_structure_get_string(s, "method");

        bMax = method == "max";
        bCompound = method == "compound";
        bIndex = method == "index";

        // TODO: create method as func to call it in depends of name
        if (!bMax && !bCompound && !bIndex)
            bMax = true;
    }

    TensorsTable convert(const OutputBlobs &output_blobs) const override;

    static std::string getName() {
        return "tensor_to_label";
    }
};
} // namespace post_processing
