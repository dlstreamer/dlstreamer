/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/buffer_mapper.h"
#include "dlstreamer/metadata.h"
#include "dlstreamer/transform.h"
#include "dlstreamer/utils.h"

#include <algorithm>
#include <map>
#include <numeric>
#include <sstream>
#include <vector>

namespace dlstreamer {

namespace param {
static constexpr auto labels = "labels";
static constexpr auto threshold = "threshold";
static constexpr auto anchors = "anchors";
static constexpr auto iou_threshold = "iou_threshold";
static constexpr auto do_cls_softmax = "do_cls_softmax";
static constexpr auto output_sigmoid_activation = "output_sigmoid_activation";
static constexpr auto cells_number = "cells_number";
static constexpr auto cells_number_x = "cells_number_x";
static constexpr auto cells_number_y = "cells_number_y";
static constexpr auto bbox_number_on_cell = "bbox_number_on_cell";
static constexpr auto params_structure = "params_structure";

static constexpr auto default_threshold = 0.5;
static constexpr auto default_iou_threshold = 0.5;
}; // namespace param

static ParamDescVector params_desc = {
    {param::labels, "Comma-separated list of object classes", ""},
    {param::threshold,
     "Detection threshold - only objects with confidence value above the threshold will be added to the frame",
     param::default_threshold, 0.0, 1.0},
    {param::anchors, "Comma-separated list of anchor values", ""},
    {param::iou_threshold, "IntersectionOverUnion threshold", param::default_iou_threshold, 0.0, 1.0},
    {param::do_cls_softmax, "If true, perform softmax", false},
    {param::output_sigmoid_activation, "output_sigmoid_activation", false},
    {param::cells_number, "Number cells (if same number along x and y axes)", 0, 0, INT32_MAX},
    {param::cells_number_x, "Number cells along x-axis", 0, 0, INT32_MAX},
    {param::cells_number_y, "Number cells along y-axis", 0, 0, INT32_MAX},
    {param::bbox_number_on_cell, "Number bounding boxes per cell", 0, 0, INT32_MAX}};

class PostProcYolo : public TransformInplace {
    static constexpr size_t default_downsample_degree = 32;

  public:
    PostProcYolo(ITransformController &transform_ctrl, DictionaryCPtr params)
        : TransformInplace(transform_ctrl, std::move(params)) {
        _labels = split_string(_params->get<std::string>(param::labels), ',');
        _classes_number = _labels.size();
        _confidence_threshold = _params->get<double>(param::threshold, param::default_threshold);
        _anchors = string_to_float_array(_params->get<std::string>(param::anchors));
        _iou_threshold = _params->get<double>(param::iou_threshold, param::default_iou_threshold);
        _do_cls_softmax = _params->get<bool>(param::do_cls_softmax, false);
        _output_sigmoid_activation = _params->get<bool>(param::output_sigmoid_activation, false);
        _cells_number_x = _cells_number_y = _params->get<int>(param::cells_number, 0);
        if (!_cells_number_x)
            _cells_number_x = _params->get<int>(param::cells_number_x, 0);
        if (!_cells_number_y)
            _cells_number_y = _params->get<int>(param::cells_number_y, 0);
        _bbox_number_on_cell = _params->get<int>(param::bbox_number_on_cell, 0);
    }

    void set_info(const BufferInfo &input_info, const BufferInfo &output_info) override {
        if (input_info.planes[0].shape != output_info.planes[0].shape)
            throw std::runtime_error("Expect same tensor shape on input and output");
        _info = input_info;
        _input_mapper = _transform_ctrl->create_input_mapper(BufferType::CPU);

        _dims_layout = getLayoutFromDims(_info, _anchors, _classes_number);

        if (!(_cells_number_x && _cells_number_y && _bbox_number_on_cell)) {
            bool is_configurated = tryAutomaticConfig();
            if (!is_configurated)
                throw std::runtime_error("Failed to deduct required parameters, please specify all parameters");
        }

        if (_anchors.size() != _bbox_number_on_cell * 2 * _info.planes.size())
            throw std::runtime_error("Anchors size must be equal (bbox_number_on_cell * layers_number * 2)");
    }

  protected:
    BufferInfo _info;
    BufferMapperPtr _input_mapper;

    enum class OutputDimsLayout { NCxCyB, NBCxCy, CxCyB, BCxCy, NO };
    enum Index : size_t { X = 0, Y = 1, W = 2, H = 3, CONFIDENCE = 4, FIRST_CLASS_PROB = 5 };

    std::vector<std::string> _labels;
    float _confidence_threshold;
    int _classes_number;
    std::vector<float> _anchors;
    double _iou_threshold;
    bool _do_cls_softmax;
    bool _output_sigmoid_activation;
    int _cells_number_x;
    int _cells_number_y;
    int _bbox_number_on_cell;
    OutputDimsLayout _dims_layout;

    static size_t tryAutomaticConfigWithDims(const std::vector<size_t> &dims, OutputDimsLayout layout, size_t boxes,
                                             size_t classes, int &cells_x, int &cells_y) {
        switch (layout) {
        case OutputDimsLayout::NBCxCy:
            cells_x = dims[2];
            cells_y = dims[3];
            break;
        case OutputDimsLayout::NCxCyB:
            cells_x = dims[1];
            cells_y = dims[2];
            break;
        case OutputDimsLayout::BCxCy:
            cells_x = dims[1];
            cells_y = dims[2];
            break;
        case OutputDimsLayout::CxCyB:
            cells_x = dims[0];
            cells_y = dims[1];
            break;

        default:
            throw std::runtime_error("Unsupported output layout.");
        }

        return cells_x * cells_y * boxes * (classes + 5);
    }

    static std::pair<std::vector<size_t>, size_t> getMinBlobDims(const BufferInfo &outputs_info) {
        auto min_size_dims = outputs_info.planes.begin()->shape;
        size_t min_blob_size = std::numeric_limits<size_t>::max();

        for (const auto &output_info_pair : outputs_info.planes) {
            const auto &blob_dims = output_info_pair.shape;

            size_t blob_size = std::accumulate(blob_dims.cbegin(), blob_dims.cend(), 1lu, std::multiplies<size_t>());
            min_blob_size = std::min(min_blob_size, blob_size);

            if (blob_size == min_blob_size)
                min_size_dims = blob_dims;
        }

        return std::pair<std::vector<size_t>, size_t>(min_size_dims, min_blob_size);
    }

    static OutputDimsLayout getLayoutFromDims(const BufferInfo &outputs_info, const std::vector<float> &anchors,
                                              size_t classes) {
        auto min_blob_dims_pair = getMinBlobDims(outputs_info);
        const auto &min_blob_dims = min_blob_dims_pair.first;

        if (min_blob_dims.size() == 1)
            return OutputDimsLayout::NO;

        size_t boxes = anchors.size() / (outputs_info.planes.size() * 2);

        const auto find_it = std::find(min_blob_dims.begin(), min_blob_dims.end(), (boxes * (classes + 5)));

        if (find_it == min_blob_dims.cend())
            return OutputDimsLayout::NO;

        size_t bbox_dim_i = std::distance(min_blob_dims.cbegin(), find_it);
        switch (min_blob_dims.size()) {
        case 3:
            switch (bbox_dim_i) {
            case 0:
                return OutputDimsLayout::BCxCy;
            case 2:
                return OutputDimsLayout::CxCyB;
            default:
                break;
            }
            break;
        case 4:
            switch (bbox_dim_i) {
            case 1:
                return OutputDimsLayout::NBCxCy;
            case 3:
                return OutputDimsLayout::NCxCyB;
            default:
                break;
            }
            break;
        default:
            break;
        }

        throw std::runtime_error("Unsupported output layout.");
    }

    bool tryAutomaticConfig() {
        _bbox_number_on_cell = _anchors.size() / (_info.planes.size() * 2);
        _cells_number_x = 0;
        _cells_number_y = 0;

        auto min_blob_dims = getMinBlobDims(_info);

        size_t batch_size = 1; // input_info.batch_size; TODO

        if (_dims_layout != OutputDimsLayout::NO) {
            size_t result_blob_size =
                tryAutomaticConfigWithDims(min_blob_dims.first, _dims_layout, _bbox_number_on_cell, _classes_number,
                                           _cells_number_x, _cells_number_y);

            if (result_blob_size * batch_size == min_blob_dims.second)
                return true;
        }

        int input_width = 416;  // TODO!!!
        int input_height = 416; // TODO!!!
        _cells_number_x = input_width / default_downsample_degree;
        _cells_number_y = input_height / default_downsample_degree;

        return min_blob_dims.second ==
               batch_size * _cells_number_x * _cells_number_y * _bbox_number_on_cell * (_classes_number + 5);
    }

    void runNms(std::vector<DetectionMetadata> &candidates) const {
        std::sort(candidates.rbegin(), candidates.rend(),
                  [](const DetectionMetadata &a, const DetectionMetadata &b) -> bool {
                      return a.confidence() < b.confidence();
                  });

        for (auto p_first_candidate = candidates.begin(); p_first_candidate != candidates.end(); ++p_first_candidate) {
            double first_x_min = p_first_candidate->x_min();
            double first_y_min = p_first_candidate->y_min();
            double first_x_max = p_first_candidate->x_max();
            double first_y_max = p_first_candidate->y_max();
            double first_candidate_area = (first_x_max - first_x_min) * (first_y_max - first_y_min);

            for (auto p_candidate = p_first_candidate + 1; p_candidate != candidates.end();) {
                double x_min = p_candidate->x_min();
                double y_min = p_candidate->y_min();
                double x_max = p_candidate->x_max();
                double y_max = p_candidate->y_max();

                const double inter_width = std::min(first_x_max, x_max) - std::max(first_x_min, x_min);
                const double inter_height = std::min(first_y_max, y_max) - std::max(first_y_min, y_min);
                if (inter_width <= 0.0 || inter_height <= 0.0) {
                    ++p_candidate;
                    continue;
                }

                const double inter_area = inter_width * inter_height;
                const double candidate_area = (x_max - x_min) * (y_max - y_min);
                const double union_area = candidate_area + first_candidate_area - inter_area;

                assert(union_area != 0 && "union_area is null. Both of the boxes have zero areas.");
                const double overlap = inter_area / union_area;
                if (overlap > _iou_threshold)
                    p_candidate = candidates.erase(p_candidate);
                else
                    ++p_candidate;
            }
        }
    }
};

} // namespace dlstreamer
