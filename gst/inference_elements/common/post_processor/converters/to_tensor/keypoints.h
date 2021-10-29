/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "blob_to_tensor_converter.h"

#include "copy_blob_to_gststruct.h"
#include "inference_backend/image_inference.h"

#include <gst/gst.h>

#include <opencv2/opencv.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace post_processing {

class KeypointsConverter : public BlobToTensorConverter {
  protected:
    const std::string format;

    template <typename T>
    void copyKeypointsToGstStructure(GstStructure *gst_struct, const std::vector<T> &points) const {
        copy_buffer_to_structure(gst_struct, reinterpret_cast<const void *>(points.data()), sizeof(T) * points.size());
    }

    GstStructure *createTensor(int precision, const std::vector<size_t> &dims) const {
        const auto &result_tensor = getModelProcOutputInfo();
        GstStructure *tensor = gst_structure_copy(result_tensor.get());

        gst_structure_set(tensor, "precision", G_TYPE_INT, precision, NULL);
        gst_structure_set(tensor, "format", G_TYPE_STRING, format.c_str(), NULL);

        GValueArray *data = g_value_array_new(dims.size());
        GValue gvalue = G_VALUE_INIT;
        g_value_init(&gvalue, G_TYPE_UINT);

        for (size_t i = 0; i < dims.size(); ++i) {
            g_value_set_uint(&gvalue, safe_convert<uint32_t>(dims[i]));
            g_value_array_append(data, &gvalue);
        }

        gst_structure_set_array(tensor, "dims", data);
        g_value_array_free(data);

        return tensor;
    }

  public:
    KeypointsConverter(BlobToMetaConverter::Initializer initializer)
        : BlobToTensorConverter(std::move(initializer)), format("keypoints") {
    }

    TensorsTable convert(const OutputBlobs &output_blobs) const = 0;
};
} // namespace post_processing
