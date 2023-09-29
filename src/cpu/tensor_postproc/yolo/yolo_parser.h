/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <dlstreamer/image_metadata.h>
#include <dlstreamer/tensor.h>

#include <cmath>

namespace dlstreamer {

/// @brief Object for parsing output of Yolo v3 and v4
class YoloParser {
  public:
    using MaskMap = std::map<size_t, std::vector<size_t>>;

    enum class Layout { NCyCxB, NBCyCx, CyCxB, BCyCx, Other };

    /// @brief Constructor of YoloParser object
    /// @param anchors Anchors array
    /// @param masks Masks array
    /// @param cells_x Number of cells along x-axis
    /// @param cells_y Number of cells along y-axis
    /// @param boxes_per_cell Number of bounding boxes per cell
    /// @param layout Layout of output tensor of yolo model
    /// @param num_classes Number of classes
    /// @param input_img_width Input image width
    /// @param input_img_height Input image height
    YoloParser(std::vector<double> anchors, const std::vector<int> &masks, size_t cells_x, size_t cells_y,
               size_t boxes_per_cell, Layout layout, size_t num_classes, size_t input_img_width,
               size_t input_img_height)
        : _anchors(std::move(anchors)), _cells_number_x(cells_x), _cells_number_y(cells_y),
          _num_bbox_on_cell(boxes_per_cell), _masks(create_masks_map(masks)), _out_shape_layout(layout),
          _num_classes(num_classes), _image_width(input_img_width), _image_height(input_img_height) {

        auto indexes = get_cells_indexes(_out_shape_layout);
        _index_cells_x = indexes.first;
        _index_cells_y = indexes.second;
    }

    virtual ~YoloParser() = default;

    static float sigmoid(float x) {
        return 1 / (1 + std::exp(-x));
    }

    /// @brief Returns minimal tensor from provided array
    /// @param infos_vec Array of TensorInfo
    /// @return
    static const TensorInfo &get_min_tensor_shape(const dlstreamer::TensorInfoVector &infos_vec);

    /// @brief Returns indexes of cells across x and y dimensions.
    /// @param layout Layout of output tensors
    /// @return Indexes in format {cells_index_x, cells_index_y} and zeroes if indexes are unknown.
    static std::pair<size_t, size_t> get_cells_indexes(Layout layout);

    static MaskMap masks_to_masks_map(const std::vector<int> &masks_flat, size_t cells_number_min,
                                      size_t bbox_number_on_cell);

    void enable_sigmoig_activation(bool enable) {
        _output_sigmoid_activation = enable;
    }

    void enable_softmax(bool enable) {
        _use_softmax = enable;
    }

    void set_confidence_threshold(double threshold) {
        _confidence_threshold = threshold;
    }

    std::vector<DetectionMetadata> parse(const Tensor &tensor) const;

  protected:
    MaskMap create_masks_map(const std::vector<int> &masks) {
        const auto num_cells_min = std::min(_cells_number_x, _cells_number_y);
        return masks_to_masks_map(masks, num_cells_min, _num_bbox_on_cell);
    }

    std::vector<DetectionMetadata> parse_blob(const float *blob, const TensorInfo &blob_info) const;

    // Calculates bounding box and retuns as tuple (x_min, y_min, x_max, y_max)
    virtual std::tuple<double, double, double, double> calc_bounding_box(size_t col, size_t row, float raw_x,
                                                                         float raw_y, float raw_w, float raw_h,
                                                                         size_t side_w, size_t side_h, size_t mask_0,
                                                                         size_t bbox_cell_num) const;

    // Converts absolute coordinates to relative using inputs size
    void convert_to_relative_coords(float &x_min, float &y_min, float &w, float &h) const;

    size_t entry_index(size_t side_square, size_t location, size_t entry) const noexcept;

    std::vector<float> softmax(const float *arr, size_t size, size_t common_offset, size_t side_square) const;

  protected:
    static constexpr size_t NUM_COORDS = 4;

    const std::vector<double> _anchors;
    const size_t _cells_number_x;
    const size_t _cells_number_y;
    const size_t _num_bbox_on_cell; // Number of bounding boxes per cell
    const MaskMap _masks;
    const Layout _out_shape_layout = Layout::Other;
    const size_t _num_classes; // Number of classes
    // Input image data
    const size_t _image_width = 0;
    const size_t _image_height = 0;

    double _confidence_threshold = 0.5;
    bool _output_sigmoid_activation = false;
    bool _use_softmax = false;

    size_t _index_cells_x = 0;
    size_t _index_cells_y = 0;
};

/// @brief Object for parsing output of Yolo v5
class Yolo5Parser : public YoloParser {
  public:
    using YoloParser::YoloParser;

    std::tuple<double, double, double, double> calc_bounding_box(size_t col, size_t row, float raw_x, float raw_y,
                                                                 float raw_w, float raw_h, size_t side_w, size_t side_h,
                                                                 size_t mask_0, size_t bbox_cell_num) const override;
};

} // namespace dlstreamer