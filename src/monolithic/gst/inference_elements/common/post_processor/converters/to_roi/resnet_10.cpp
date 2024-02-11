/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "resnet_10.h"

#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "safe_arithmetic.hpp"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iostream>


#define CLIP(a,min,max) (MAX(MIN(a, max), min))
#define DIVIDE_AND_ROUND_UP(a, b) ((a + b - 1) / b)

constexpr int kNUM_CONFIGURED_CLASSES = 4;


using namespace post_processing;

void Resnet10Converter::parseOutputBlob(const InferDimsCHW& covLayerDims, const InferDimsCHW& bboxLayerDims, const float *outputCovBuf, const float *outputBboxBuf,
                                      int numClassesToParse, std::vector<DetectedObject> &objects) const {

  int gridW = covLayerDims.w;
  int gridH = covLayerDims.h;
  int gridSize = gridW * gridH;
  float gcCentersX[gridW];
  float gcCentersY[gridH];
  float bboxNormX = 35.0;
  float bboxNormY = 35.0;
  size_t input_width = getModelInputImageInfo().width;
  size_t input_height = getModelInputImageInfo().height;
  int strideX = DIVIDE_AND_ROUND_UP(input_width, bboxLayerDims.w);
  int strideY = DIVIDE_AND_ROUND_UP(input_height, bboxLayerDims.h);

  for (int i = 0; i < gridW; i++)
  {
    gcCentersX[i] = (float)(i * strideX + 0.5);
    gcCentersX[i] /= (float)bboxNormX;

  }
  for (int i = 0; i < gridH; i++)
  {
    gcCentersY[i] = (float)(i * strideY + 0.5);
    gcCentersY[i] /= (float)bboxNormY;

  }

  for (int c = 0; c < numClassesToParse; c++)
  {
    const float *outputX1 = outputBboxBuf + (c * 4 * bboxLayerDims.h * bboxLayerDims.w);

    const float *outputY1 = outputX1 + gridSize;
    const float *outputX2 = outputY1 + gridSize;
    const float *outputY2 = outputX2 + gridSize;

    if(c >= kNUM_CONFIGURED_CLASSES) throw std::runtime_error("class id " + std::to_string(c) + " is out of bound");

    for (int h = 0; h < gridH; h++)
    {
      for (int w = 0; w < gridW; w++)
      {
        int i = w + h * gridW;
        if (outputCovBuf[c * gridSize + i] >= confidence_threshold)
        {
          float rectX1f, rectY1f, rectX2f, rectY2f;

          rectX1f = (outputX1[w + h * gridW] - gcCentersX[w]) * -bboxNormX;
          rectY1f = (outputY1[w + h * gridW] - gcCentersY[h]) * -bboxNormY;
          rectX2f = (outputX2[w + h * gridW] + gcCentersX[w]) * bboxNormX;
          rectY2f = (outputY2[w + h * gridW] + gcCentersY[h]) * bboxNormY;

          float x = CLIP(rectX1f, 0, input_width - 1);
          float y = CLIP(rectY1f, 0, input_height - 1);
          float w = CLIP(rectX2f, 0, input_width - 1) - x + 1;
          float h = CLIP(rectY2f, 0, input_height - 1) - y + 1;

          objects.push_back(DetectedObject(x, y, w, h, outputCovBuf[c * gridSize + i], c,
                                             BlobToMetaConverter::getLabelByLabelId(c), 1.0f / input_width,
                                             1.0f / input_height, false));
        }
      }
    }
  }

}

TensorsTable Resnet10Converter::convert(const OutputBlobs &output_blobs) const {
    ITT_TASK(__FUNCTION__);
    try {

        static InferDimsCHW covLayerDims = {0, 0, 0};
        static InferDimsCHW bboxLayerDims = {0, 0, 0};
        int numClassesToParse;

        const auto &model_input_image_info = getModelInputImageInfo();
        size_t batch_size = model_input_image_info.batch_size;

        DetectedObjectsTable objects_table(batch_size);

        for (size_t batch_number = 0; batch_number < batch_size; ++batch_number) {
            auto &objects = objects_table[batch_number];
            const float *outputCovBuf = nullptr;
            const float *outputBboxBuf = nullptr;
            for (const auto &blob_iter : output_blobs) {
                const InferenceBackend::OutputBlob::Ptr &blob = blob_iter.second;
                if (not blob)
                    throw std::invalid_argument("Output blob is nullptr.");

                size_t unbatched_size = blob->GetSize() / batch_size;
                if(blob_iter.first == "conv2d_bbox")
                {
                    if(!bboxLayerDims.c)
                    {
                        const auto& dims = blob->GetDims(); // NCHW
                        bboxLayerDims.set(dims[1], dims[2], dims[3]);
                    }
                    outputBboxBuf = reinterpret_cast<const float *>(blob->GetData()) + unbatched_size * batch_number;
                }
                if(blob_iter.first == "conv2d_cov/Sigmoid")
                {
                    if(!covLayerDims.c)
                    {
                        const auto& dims = blob->GetDims();
                        covLayerDims.set(dims[1], dims[2], dims[3]);
                    }
                    outputCovBuf = reinterpret_cast<const float *>(blob->GetData()) + unbatched_size * batch_number;
                }
            }

            if(!outputCovBuf || !outputCovBuf) throw std::runtime_error("Failed to do Resnet10 post-processing.");

            numClassesToParse = MIN(covLayerDims.c, kNUM_CONFIGURED_CLASSES);
            parseOutputBlob(covLayerDims, bboxLayerDims, outputCovBuf, outputBboxBuf, numClassesToParse, objects);
        }

        return storeObjects(objects_table);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do Resnet10 post-processing."));
    }
    return TensorsTable{};
}
