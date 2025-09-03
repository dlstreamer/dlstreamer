/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/opencv/elements/opencv_find_contours.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/cpu/tensor.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/utils.h"
#include "opencv2/imgproc.hpp"

using namespace cv;
using namespace std;

namespace dlstreamer {

namespace param {
static constexpr auto mask_metadata_name = "mask_metadata_name";
static constexpr auto contour_metadata_name = "contour_metadata_name";
static constexpr auto threshold = "threshold";

static constexpr auto mask_metadata_default_name = "mask";
static constexpr auto contour_metadata_default_name = "contour";
static constexpr auto default_threshold = 0.5;
}; // namespace param

static ParamDescVector params_desc = {
    {param::mask_metadata_name, "Name of metadata containing segmentation mask", param::mask_metadata_default_name},
    {param::contour_metadata_name, "Name of metadata created by this element to store contour(s)",
     param::contour_metadata_default_name},
    {param::threshold,
     "Mask threshold - only mask pixels with confidence values above the threshold will be used for finding contours",
     param::default_threshold, 0.0, 1.0}};

class OpencvFindContours : public BaseTransformInplace {
  public:
    static constexpr auto mask_format = "mask";
    static constexpr auto contour_format = "contour_points";

    OpencvFindContours(DictionaryCPtr params, const ContextPtr &app_context) : BaseTransformInplace(app_context) {
        _mask_metadata_name = params->get<std::string>(param::mask_metadata_name, param::mask_metadata_default_name);
        _contour_metadata_name =
            params->get<std::string>(param::contour_metadata_name, param::contour_metadata_default_name);
        _mask_threshold = params->get<double>(param::threshold, param::default_threshold);
    }

    bool process(FramePtr src) override {

        for (auto &region : src->regions()) {
            auto mask_meta = find_metadata<InferenceResultMetadata>(*region, _mask_metadata_name, mask_format);
            if (!mask_meta)
                return true;
            auto mask_tensor = mask_meta->tensor();
            float *mask_data = mask_tensor->data<float>();
            ImageInfo mask_info(mask_tensor->info());
            DLS_CHECK(mask_info.info().is_contiguous());
            int mask_width = mask_info.width();
            int mask_height = mask_info.height();

            cv::Mat bitmask(mask_height, mask_width, CV_8UC1);
            uint8_t *bitmask_data = (uint8_t *)(bitmask.data);
            for (int i = 0; i < mask_height * mask_width; i++)
                bitmask_data[i] = (mask_data[i] >= _mask_threshold) ? 1 : 0;
            vector<vector<Point>> contours;
            findContours(bitmask, contours, RETR_TREE, CHAIN_APPROX_SIMPLE);
            for (auto &contour : contours) {
                size_t num_points = contour.size();
                std::vector<std::vector<float>> normalized_points;
                for (size_t i = 0; i < num_points; i++) {
                    normalized_points[i][0] = ((float)contour[i].x) / mask_width;
                    normalized_points[i][1] = ((float)contour[i].y) / mask_height;
                }
                CPUTensor contour_tensor({{num_points, 2}, DataType::Float32}, normalized_points.data());
                auto contour_meta = add_metadata<InferenceResultMetadata>(*region, _contour_metadata_name);
                contour_meta.init_tensor_data(contour_tensor, "", contour_format);
            }
        }
        return true;
    }

  private:
    std::string _mask_metadata_name;
    std::string _contour_metadata_name;
    float _mask_threshold;
};

extern "C" {
ElementDesc opencv_find_contours = {.name = "opencv_find_contours",
                                    .description = "Find contour points of given mask using opencv",
                                    .author = "Intel Corporation",
                                    .params = &params_desc,
                                    .input_info = MAKE_FRAME_INFO_VECTOR({MediaType::Any}),
                                    .output_info = MAKE_FRAME_INFO_VECTOR({MediaType::Any}),
                                    .create = create_element<OpencvFindContours>,
                                    .flags = 0};
}

} // namespace dlstreamer
