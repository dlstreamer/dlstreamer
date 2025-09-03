/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/opencv/elements/opencv_remove_background.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/cpu/context.h"
#include "dlstreamer/cpu/tensor.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/opencv/context.h"
#include "dlstreamer/opencv/mappers/cpu_to_opencv.h"
#include "dlstreamer/opencv/tensor.h"
#include "dlstreamer/utils.h"
#include "opencv2/imgproc.hpp"

using namespace cv;
using namespace std;

namespace dlstreamer {

namespace param {
static constexpr auto mask_metadata_name = "mask_metadata_name";
static constexpr auto threshold = "threshold";

static constexpr auto mask_metadata_default_name = "mask";
static constexpr auto default_threshold = 0.5;
}; // namespace param

static ParamDescVector params_desc = {
    {param::mask_metadata_name, "Name of metadata containing segmentation mask", param::mask_metadata_default_name},
    {param::threshold,
     "Mask threshold - only mask pixels with confidence values above the threshold will be used for setting "
     "transparency",
     param::default_threshold, 0.0, 1.0}};

class OpencvRemoveBackground : public BaseTransformInplace {
  public:
    static constexpr auto mask_format = "mask";

    OpencvRemoveBackground(DictionaryCPtr params, const ContextPtr &app_context) : BaseTransformInplace(app_context) {
        _mask_metadata_name = params->get<std::string>(param::mask_metadata_name, param::mask_metadata_default_name);
        _mask_threshold = params->get<double>(param::threshold, param::default_threshold);
    }

    bool init_once() override {
        auto cpu_context = std::make_shared<CPUContext>();
        auto opencv_context = std::make_shared<OpenCVContext>();
        _opencv_mapper = create_mapper({_app_context, cpu_context, opencv_context});
        return true;
    }

    bool process(FramePtr frame) override {
        DLS_CHECK(init());
        auto cv_tensor = ptr_cast<OpenCVTensor>(_opencv_mapper->map(frame->tensor(), AccessMode::ReadWrite));
        Mat cv_mat = *cv_tensor;

        // roi_id of current ROI
        auto source_id_meta = find_metadata<SourceIdentifierMetadata>(*frame);
        if (!source_id_meta)
            throw std::runtime_error("SourceIdentifierMetadata not found");
        int roi_id = source_id_meta->roi_id();

        // find mask tensor by roi_id
        TensorPtr mask_tensor;
        for (auto &region : frame->regions()) {
            auto detection_meta = find_metadata<DetectionMetadata>(*region);
            if (!detection_meta || detection_meta->id() != roi_id)
                continue;
            auto mask_meta = find_metadata<InferenceResultMetadata>(*region, _mask_metadata_name, mask_format);
            if (!mask_meta)
                continue;
            mask_tensor = mask_meta->tensor();
            break;
        }
        if (!mask_tensor)
            throw std::runtime_error("mask metadata not found");

        float *mask_data = mask_tensor->data<float>();
        ImageInfo mask_info(mask_tensor->info());
        DLS_CHECK(mask_info.info().is_contiguous());
        int mask_width = mask_info.width();
        int mask_height = mask_info.height();

        cv::Mat bitmask(mask_height, mask_width, CV_8UC1);
        uint8_t *bitmask_data = (uint8_t *)(bitmask.data);
        for (int i = 0; i < mask_height * mask_width; i++)
            bitmask_data[i] = (mask_data[i] >= _mask_threshold) ? 255 : 0;

        Mat resized_mask;
        resize(bitmask, resized_mask, cv_mat.size(), 0, 0, INTER_NEAREST);
        if (cv_mat.channels() == 3)
            cvtColor(resized_mask, resized_mask, COLOR_GRAY2RGB);
        else if (cv_mat.channels() == 4)
            cvtColor(resized_mask, resized_mask, COLOR_GRAY2RGBA);
        else
            throw std::runtime_error("Unsupported number channels");

        cv::bitwise_and(cv_mat, resized_mask, cv_mat);
        return true;
    }

  private:
    MemoryMapperPtr _opencv_mapper;
    std::string _mask_metadata_name;
    float _mask_threshold;
};

extern "C" {
ElementDesc opencv_remove_background = {.name = "opencv_remove_background",
                                        .description = "Remove background using mask",
                                        .author = "Intel Corporation",
                                        .params = &params_desc,
                                        .input_info = MAKE_FRAME_INFO_VECTOR({
                                            {ImageFormat::RGB},
                                            {ImageFormat::BGR},
                                            {ImageFormat::RGBX},
                                            {ImageFormat::BGRX},
                                        }),
                                        .output_info = MAKE_FRAME_INFO_VECTOR({
                                            {ImageFormat::RGB},
                                            {ImageFormat::BGR},
                                            {ImageFormat::RGBX},
                                            {ImageFormat::BGRX},
                                        }),
                                        .create = create_element<OpencvRemoveBackground>,
                                        .flags = 0};
}

} // namespace dlstreamer
