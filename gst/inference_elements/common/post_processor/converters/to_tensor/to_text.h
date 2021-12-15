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

class ToTextConverter : public BlobToTensorConverter {
    double scale = 1.0;
    int precision = 2;

  public:
    ToTextConverter(const std::string &model_name, const ModelImageInputInfo &input_image_info,
                    GstStructureUniquePtr model_proc_output_info, const std::vector<std::string> &labels)
        : BlobToTensorConverter(model_name, input_image_info, std::move(model_proc_output_info), labels) {
        if (!raw_tesor_copying->enabled(RawTensorCopyingToggle::id))
            GVA_WARNING(RawTensorCopyingToggle::deprecation_message.c_str());

        GstStructure *s = getModelProcOutputInfo().get();

        gst_structure_get_double(s, "tensor_to_text_scale", &scale);
        gst_structure_get_int(s, "tensor_to_text_precision", &precision);
    }

    TensorsTable convert(const OutputBlobs &output_blobs) const override;

    static std::string getName() {
        return "tensor_to_text";
    }
};
} // namespace post_processing
