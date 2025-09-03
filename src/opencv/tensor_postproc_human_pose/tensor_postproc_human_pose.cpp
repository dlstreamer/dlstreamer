/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/opencv/elements/tensor_postproc_human_pose.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/cpu/tensor.h"
#include "dlstreamer/cpu/utils.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/opencv/mappers/cpu_to_opencv.h"
#include "opencv2/imgproc.hpp"
#include "peak.h"

namespace dlstreamer {

namespace param {
static constexpr auto point_names = "point-names";
static constexpr auto point_connections = "point-connections";
} // namespace param

static ParamDescVector params_desc = {{param::point_names, "Array of key point names", std::vector<std::string>()},
                                      {param::point_connections,
                                       "Array of point connections {name-A0, name-B0, name-A1, name-B1, ...}",
                                       std::vector<std::string>()}};

namespace dflt { // TODO add to parameters?
static constexpr int upsample_ratio = 4;

static constexpr int min_joints_number = 3;
static constexpr float min_peaks_distance = 3.0;
static constexpr float mid_points_score_threshold = 0.05;
static constexpr float found_mid_points_ratio_threshold = 0.8;
static constexpr float min_subset_score = 0.2;
} // namespace dflt

class TensorPostProcHumanPose : public BaseTransformInplace {
  public:
    TensorPostProcHumanPose(DictionaryCPtr params, const ContextPtr &app_context) : BaseTransformInplace(app_context) {
        _point_names = params->get(param::point_names, std::vector<std::string>());
        _point_connections = params->get(param::point_connections, std::vector<std::string>());
        _keypoints_number = _point_names.size();
    }

    bool process(FramePtr src) override {
        if (_heatmap_index < 0 || _paf_index < 0)
            auto_detect(src);

        auto frame = src.map(AccessMode::Read);

        HumanPoses poses = get_human_poses(frame);

        correct_coordinates(poses, _feature_size);

        for (const auto &pose : poses) {
            CPUTensor pose_tensor({{pose.keypoints.size(), 2}, DataType::Float32}, (void *)pose.keypoints.data());
            auto meta = add_metadata<InferenceResultMetadata>(*src, "keypoints");
            meta.init_tensor_data(pose_tensor, "keypoints", "keypoints");
            if (!_point_names.empty())
                meta.set("point_names", _point_names);
            if (!_point_connections.empty())
                meta.set("point_connections", _point_connections);
            if (!_model_name.empty())
                meta.set_model_name(_model_name);
            if (!_layer_name.empty())
                meta.set_layer_name(_layer_name);
        }
        return true;
    }

  private:
    void auto_detect(const FramePtr &frame) {
        DLS_CHECK(frame->num_tensors() == 2)
        auto shape0 = frame->tensor(0)->info().shape;
        auto shape1 = frame->tensor(1)->info().shape;
        DLS_CHECK(shape0.size() == 4)
        DLS_CHECK(shape1.size() == 4)
        DLS_CHECK(shape0[0] == shape1[0])
        DLS_CHECK(shape0[2] == shape1[2])
        DLS_CHECK(shape0[3] == shape1[3])
        if (shape0[1] == shape1[1] * 2) {
            _heatmap_index = 1;
            _paf_index = 0;
        } else if (shape1[1] == shape0[1] * 2) {
            _heatmap_index = 0;
            _paf_index = 1;
        } else
            throw std::runtime_error("Unsupported output shapes");

        if (!_keypoints_number)
            _keypoints_number = std::min(shape0[1], shape1[1]) - 1;
        _feature_size = {static_cast<int>(shape0[3]), static_cast<int>(shape0[2])};

        auto model_info = find_metadata<ModelInfoMetadata>(*frame);
        if (model_info) {
            _model_name = model_info->model_name();
            auto output_layers = model_info->output_layers();
            _layer_name = join_strings(output_layers.cbegin(), output_layers.cend(), '\\');
        }
    }

    HumanPoses get_human_poses(const FramePtr &frame) {
        TensorPtr pafs_tensor = frame->tensor(_paf_index);
        TensorPtr heatmap_tensor = frame->tensor(_heatmap_index);

        // upsample heat_maps
        std::vector<cv::Mat> heat_maps(heatmap_tensor->info().shape[1]);
        for (size_t i = 0; i < heat_maps.size(); i++) {
            auto slice = get_tensor_slice(heatmap_tensor, {{0, 1}, {i, 1}, {}, {}}, true);
            cv::Mat mat = ptr_cast<OpenCVTensor>(_opencv_mapper.map(slice, AccessMode::Read))->cv_mat();
            cv::resize(mat, heat_maps[i], cv::Size(), dflt::upsample_ratio, dflt::upsample_ratio, cv::INTER_CUBIC);
        }

        // upsample pafs
        std::vector<cv::Mat> pafs(pafs_tensor->info().shape[1]);
        for (size_t i = 0; i < pafs.size(); i++) {
            auto slice = get_tensor_slice(pafs_tensor, {{0, 1}, {i, 1}, {}, {}}, true);
            cv::Mat mat = ptr_cast<OpenCVTensor>(_opencv_mapper.map(slice, AccessMode::Read))->cv_mat();
            cv::resize(mat, pafs[i], cv::Size(), dflt::upsample_ratio, dflt::upsample_ratio, cv::INTER_CUBIC);
        }

        // find peaks in heat_maps
        std::vector<std::vector<Peak>> peaks_from_heat_map(heat_maps.size());
        FindPeaksBody find_peaks_body(heat_maps, dflt::min_peaks_distance, peaks_from_heat_map);
        cv::parallel_for_(cv::Range(0, static_cast<int>(heat_maps.size())), find_peaks_body);
        int peaks_before = 0;
        for (size_t heatmap_id = 1; heatmap_id < heat_maps.size(); heatmap_id++) {
            peaks_before += static_cast<int>(peaks_from_heat_map[heatmap_id - 1].size());
            for (auto &peak : peaks_from_heat_map[heatmap_id]) {
                peak.id += peaks_before;
            }
        }

        return GroupPeaksToPoses(peaks_from_heat_map, pafs, _keypoints_number, dflt::mid_points_score_threshold,
                                 dflt::found_mid_points_ratio_threshold, dflt::min_joints_number,
                                 dflt::min_subset_score);
    }

    void correct_coordinates(HumanPoses &poses, cv::Size output_feature_map_size) const {
        // output network image size
        cv::Size full_feature_map_size = output_feature_map_size * dflt::upsample_ratio;
        for (auto &pose : poses) {
            for (auto &keypoint : pose.keypoints) {
                if (keypoint != cv::Point2f(-1, -1)) {
                    // transfer keypoint from output to original image
                    keypoint.x /= static_cast<float>(full_feature_map_size.width);
                    keypoint.y /= static_cast<float>(full_feature_map_size.height);
                }
            }
        }
    }

  protected:
    int _heatmap_index = -1;
    int _paf_index = -1;
    size_t _keypoints_number = 0;
    std::vector<std::string> _point_names;
    std::vector<std::string> _point_connections;
    std::string _model_name;
    std::string _layer_name;
    cv::Size _feature_size;
    MemoryMapperCPUToOpenCV _opencv_mapper;
};

extern "C" {
ElementDesc tensor_postproc_human_pose = {
    .name = "tensor_postproc_human_pose",
    .description = "Post-processing to extract key points from human pose estimation model output",
    .author = "Intel Corporation",
    .params = &params_desc,
    .input_info = MAKE_FRAME_INFO_VECTOR({MediaType::Tensors}),
    .output_info = MAKE_FRAME_INFO_VECTOR({MediaType::Tensors}),
    .create = create_element<TensorPostProcHumanPose>,
    .flags = 0};
}

} // namespace dlstreamer
