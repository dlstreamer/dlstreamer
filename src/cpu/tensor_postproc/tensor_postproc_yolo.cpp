/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "yolo/yolo_parser.h"

#include "dlstreamer/base/transform.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/transform.h"
#include "dlstreamer/utils.h"
#include "dlstreamer_logger.h"
#include "load_labels_file.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <mutex>
#include <numeric>
#include <sstream>
#include <vector>

#include <spdlog/fmt/ranges.h>

namespace dlstreamer {

namespace param {
static constexpr auto yolo_version = "version";
static constexpr auto labels = "labels";
static constexpr auto labels_file = "labels-file";
static constexpr auto threshold = "threshold";
static constexpr auto anchors = "anchors";
static constexpr auto masks = "masks";
static constexpr auto iou_threshold = "iou-threshold";
static constexpr auto do_cls_softmax = "do-cls-softmax";
static constexpr auto output_sigmoid_activation = "output-sigmoid-activation";
static constexpr auto cells_number = "cells-number";
static constexpr auto cells_number_x = "cells-number-x";
static constexpr auto cells_number_y = "cells-number-y";
static constexpr auto bbox_number_on_cell = "bbox-number-on-cell";
static constexpr auto classes = "classes";
static constexpr auto nms = "nms";

static constexpr auto default_threshold = 0.5;
static constexpr auto default_iou_threshold = 0.5;
static constexpr auto default_softmax_enabled = true;
static constexpr auto default_sigmoid_activation = true;
static constexpr auto default_nms = true;
}; // namespace param

static ParamDescVector params_desc = {
    {param::yolo_version, "Yolo's version number. Supported only from 3 to 5", 0, 0, 5}, // TODO: Make a dictionary
    {param::labels, "Array of object classes", std::vector<std::string>()},
    {param::labels_file, "Path to .txt file containing object classes (one per line)", std::string()},
    {param::threshold,
     "Detection threshold - only objects with confidence value above the threshold will be added to the frame",
     param::default_threshold, 0.0, 1.0},
    {param::anchors, "Anchor values array", std::vector<double>()},
    {param::masks, "Masks values array (1 dimension)", std::vector<int>()},
    {param::iou_threshold, "IntersectionOverUnion threshold", param::default_iou_threshold, 0.0, 1.0},
    {param::do_cls_softmax, "If true, perform softmax", param::default_softmax_enabled},
    {param::output_sigmoid_activation, "output_sigmoid_activation", param::default_sigmoid_activation},
    {param::cells_number, "Number of cells. Use if number of cells along x and y axes is the same (0 = autodetection)",
     0, 0, INT32_MAX},
    {param::cells_number_x, "Number of cells along x-axis", 0, 0, INT32_MAX},
    {param::cells_number_y, "Number of cells along y-axis", 0, 0, INT32_MAX},
    {param::bbox_number_on_cell, "Number of bounding boxes that can be predicted per cell (0 = autodetection)", 0, 0,
     INT32_MAX},
    {param::classes, "Number of classes", 0, 0, INT32_MAX},
    {param::nms, "Apply Non-Maximum Suppression (NMS) filter to bounding boxes", param::default_nms}};

class YoloParserBuilder final {
  public:
    using Layout = YoloParser::Layout;

    bool is_yolo_version_supported() const {
        return _yolo_version >= 3 && _yolo_version <= 5;
    }

    void set_logger(std::shared_ptr<spdlog::logger> logger) {
        _logger = logger;
    }

    void set_params(DictionaryCPtr params, size_t num_labels) {
        _yolo_version = params->get<int>(param::yolo_version, 0);
        if (!is_yolo_version_supported())
            throw std::runtime_error(fmt::format("Yolo version {} is not supported", _yolo_version));

        _num_classes = params->get<int>(param::classes, 0);
        if (!_num_classes) {
            // DLS_CHECK(num_labels);
            if (num_labels)
                _num_classes = num_labels;
            else
                _num_classes = 80; // default YOLO dataset is COCO with 80 classes
        } else {
            if (num_labels && num_labels != _num_classes)
                throw std::logic_error(fmt::format("Number of classes ({}) is not equal to the number of labels ({})",
                                                   _num_classes, num_labels));
        }
        _num_cells_x = params->get<int>(param::cells_number_x, 0);
        if (!_num_cells_x)
            _num_cells_x = params->get<int>(param::cells_number, 0);
        _num_cells_y = params->get<int>(param::cells_number_y, 0);
        if (!_num_cells_y)
            _num_cells_y = params->get<int>(param::cells_number, 0);
        _num_bbox_on_cell = params->get<int>(param::bbox_number_on_cell, 0);

        _anchors = params->get<std::vector<double>>(param::anchors, {});
        if (_anchors.empty()) {
            switch (_yolo_version) {
            case 3:
            case 5:
                _anchors = {10.0, 13.0, 16.0,  30.0,  33.0, 23.0,  30.0,  61.0,  62.0,
                            45.0, 59.0, 119.0, 116.0, 90.0, 156.0, 198.0, 373.0, 326.0};
                break;
            case 4:
                _anchors = {12.0, 16.0, 19.0,  36.0,  40.0,  28.0,  36.0,  75.0,  76.0,
                            55.0, 72.0, 146.0, 142.0, 110.0, 192.0, 243.0, 459.0, 401.0};
                break;
            default:
                throw std::runtime_error(fmt::format("Default anchors on version {} not supported", _yolo_version));
            }
        }
        _masks = params->get<std::vector<int>>(param::masks, {});
        if (_masks.empty()) {
            _masks = {6, 7, 8, 3, 4, 5, 0, 1, 2};
        }

        _sigmoid_activation_enabled =
            params->get<bool>(param::output_sigmoid_activation, param::default_sigmoid_activation);
        _softmax_enabled = params->get<bool>(param::do_cls_softmax, param::default_softmax_enabled);
        _threshold = params->get<double>(param::threshold, param::default_threshold);
    }

    void set_out_shapes(const TensorInfoVector &out_info) {
        _out_info = out_info;
    }

    void set_image_info(size_t width, size_t height) {
        _image_width = width;
        _image_height = height;
    }

    // Builds a new parser based on configured parameters
    std::unique_ptr<YoloParser> build() {
        if (!_logger)
            throw std::runtime_error("Builder: Logger object is required");

        if (_out_info.empty())
            throw std::runtime_error("Builder: output shapes must be specified");

        if (!is_yolo_version_supported()) {
            throw std::runtime_error(fmt::format("Builder: Yolo version {} is not supported", _yolo_version));
        }

        // Is it required to call build several times changing parameters in between?
        assert(!_dirty && "Don't call build twice with different parameters - it won't process any changes");
        _dirty = true;

        Layout layout = detect_out_shapes_layout();

        const bool need_auto_configuration = !(_num_cells_x && _num_cells_y && _num_bbox_on_cell);
        if (need_auto_configuration) {
            if (!try_auto_configure(layout)) {
                throw std::runtime_error(
                    "Builder: Failed to automatically determine parameters. Please specify parameters manually");
            }

            // Make sure we have some non-zero values.
            assert(_num_cells_x && _num_cells_y && _num_bbox_on_cell);
            _logger->info("Auto-configuration result: number of cells x={} y={}, number of bboxes per cell={}",
                          _num_cells_x, _num_cells_y, _num_bbox_on_cell);
        }

        verify_parameters(layout);

        auto parser = create_parser(layout);

        _logger->info("Yolo parser additional parameters: softmax={}, sigmoid_activation={}, threshold={}",
                      _softmax_enabled, _sigmoid_activation_enabled, _threshold);
        parser->enable_softmax(_softmax_enabled);
        parser->enable_sigmoig_activation(_sigmoid_activation_enabled);
        parser->set_confidence_threshold(_threshold);

        return parser;
    }

  protected:
    std::unique_ptr<YoloParser> create_parser(Layout layout) const {
        assert(is_yolo_version_supported() && "Invalid Yolo's version");
        if (_yolo_version == 5)
            return create_parser<Yolo5Parser>(layout);
        else
            return create_parser<YoloParser>(layout);
    }

    template <class ParserTy>
    std::unique_ptr<ParserTy> create_parser(Layout layout) const {
        _logger->info("Yolo parser create: version={}, num_cells_x={}, num_cells_y={}, num_bbox_on_cell={}, layout={}, "
                      "num_classes={}, image_width={}, image_height={}",
                      _yolo_version, _num_cells_x, _num_cells_y, _num_bbox_on_cell, static_cast<int>(layout),
                      _num_classes, _image_width, _image_height);
        return std::make_unique<ParserTy>(_anchors, _masks, _num_cells_x, _num_cells_y, _num_bbox_on_cell, layout,
                                          _num_classes, _image_width, _image_height);
    }

    size_t get_boxes_count() const noexcept {
        assert(!_out_info.empty() && "Output info must be set");
        if (_out_info.empty())
            return 0;
        return _anchors.size() / (_out_info.size() * 2);
    }

    Layout detect_out_shapes_layout() const {
        const TensorInfo &min_tensor_info = YoloParser::get_min_tensor_shape(_out_info);
        const auto &min_blob_dims = min_tensor_info.shape;

        if (min_blob_dims.size() == 1)
            return Layout::Other;

        const size_t boxes = get_boxes_count();

        const auto find_it = std::find(min_blob_dims.begin(), min_blob_dims.end(), (boxes * (_num_classes + 5)));

        if (find_it == min_blob_dims.cend()) {
            return Layout::Other;
        }

        size_t bbox_dim_i = std::distance(min_blob_dims.cbegin(), find_it);
        switch (min_blob_dims.size()) {
        case 3:
            switch (bbox_dim_i) {
            case 0:
                return Layout::BCyCx;
            case 2:
                return Layout::CyCxB;
            default:
                break;
            }
            break;
        case 4:
            switch (bbox_dim_i) {
            case 1:
                return Layout::NBCyCx;
            case 3:
                return Layout::NCyCxB;
            default:
                break;
            }
            break;
        default:
            break;
        }

        throw std::runtime_error(fmt::format("Unsupported layout of output shape: {}", min_blob_dims));
    }

    bool try_auto_configure(Layout layout) {
        size_t boxes = get_boxes_count();
        const TensorInfo &min_tensor_info = YoloParser::get_min_tensor_shape(_out_info);
        _logger->info("Auto-configuration: layout={}, boxes count={}, min shape={}", static_cast<int>(layout), boxes,
                      min_tensor_info.shape);

        if (layout != Layout::Other) {
            auto [ix, iy] = YoloParser::get_cells_indexes(layout);
            auto cells_x = min_tensor_info.shape[ix];
            auto cells_y = min_tensor_info.shape[iy];

            size_t result_blob_size = cells_x * cells_y * boxes * (_num_classes + 5);
            if (result_blob_size * _batch_size == min_tensor_info.size()) {
                _num_cells_x = cells_x;
                _num_cells_y = cells_y;
                _num_bbox_on_cell = boxes;
                return true;
            }
        }

        size_t cells_number_x = _image_width / _downsample_degree;
        size_t cells_number_y = _image_height / _downsample_degree;

        _logger->info(
            "Auto-configuration: trying number of cells x={}, y={}. Input parameters: image w={}, h={}, downsample={}",
            cells_number_x, cells_number_y, _image_width, _image_height, _downsample_degree);
        bool ok = min_tensor_info.size() == _batch_size * cells_number_x * cells_number_y * boxes * (_num_classes + 5);
        if (ok) {
            _num_cells_x = cells_number_x;
            _num_cells_y = cells_number_y;
            _num_bbox_on_cell = boxes;
        }

        return ok;
    }

    void verify_parameters(Layout layout) {
        const TensorInfo &min_tensor_info = YoloParser::get_min_tensor_shape(_out_info);

        const size_t estimated_blob_size =
            _batch_size * _num_cells_x * _num_cells_y * _num_bbox_on_cell * (_num_classes + 5);

        if (min_tensor_info.size() != estimated_blob_size) {
            auto msg = fmt::format("Builder: Size of the NN output tensor ({}) does not match the estimated ({})",
                                   min_tensor_info.size(), estimated_blob_size);
            throw std::runtime_error(msg);
        }

        auto [idx_cells_x, idx_cells_y] = YoloParser::get_cells_indexes(layout);
        if (!idx_cells_x && !idx_cells_y)
            return;

        const auto masks_map =
            YoloParser::masks_to_masks_map(_masks, std::min(_num_cells_x, _num_cells_y), _num_bbox_on_cell);

        for (const auto &info : _out_info) {
            size_t min_side = std::min(info.shape[idx_cells_x], info.shape[idx_cells_y]);
            auto it = masks_map.find(min_side);
            if (it == masks_map.end()) {
                auto msg = fmt::format(
                    "Builder: Mismatch between the size of the bounding box in the mask: {} - and the actual of the "
                    "bounding box: {}",
                    masks_map.cbegin()->first, min_side);

                throw std::runtime_error(msg);
            }
        }

        if (_num_cells_x != min_tensor_info.shape[idx_cells_x]) {
            auto msg = fmt::format(
                "Builder: Mismatch between number of cells along X ({}) - and the actual of the bounding box ({})",
                _num_cells_x, min_tensor_info.shape[idx_cells_x]);

            throw std::runtime_error(msg);
        }

        if (_num_cells_y != min_tensor_info.shape[idx_cells_y]) {
            auto msg = fmt::format(
                "Builder: Mismatch between number of cells along Y ({}) - and the actual of the bounding box: {}",
                _num_cells_y, min_tensor_info.shape[idx_cells_y]);

            throw std::runtime_error(msg);
        }
    }

  private:
    size_t _yolo_version = 3;
    size_t _num_cells_x = 0;
    size_t _num_cells_y = 0;
    size_t _num_classes = 0;
    size_t _num_bbox_on_cell = 0;
    double _threshold = param::default_threshold;
    std::vector<double> _anchors;
    std::vector<int> _masks;
    TensorInfoVector _out_info;
    bool _softmax_enabled = param::default_softmax_enabled;
    bool _sigmoid_activation_enabled = param::default_sigmoid_activation;

    size_t _image_width = 0;
    size_t _image_height = 0;

    std::shared_ptr<spdlog::logger> _logger;
    size_t _batch_size = 1; // TODO

    size_t _downsample_degree = 32; // Default downsample degree

    bool _dirty = false;
};

class PostProcYolo : public BaseTransformInplace {
  public:
    PostProcYolo(DictionaryCPtr params, const ContextPtr &app_context)
        : BaseTransformInplace(app_context),
          _logger(log::get_or_nullsink(params->get(param::logger_name, std::string()))) {
        _labels = params->get(param::labels, std::vector<std::string>());
        auto labels_file = params->get(param::labels_file, std::string());
        if (!labels_file.empty())
            _labels = load_labels_file(labels_file);
        _apply_nms = params->get<bool>(param::nms, param::default_nms);
        _iou_threshold = params->get<double>(param::iou_threshold, param::default_iou_threshold);
        // other params passed to builder
        _builder.set_logger(_logger);
        _builder.set_params(params, _labels.size());
    }

    void set_info(const FrameInfo &info) override {
        _info = info;
        _builder.set_out_shapes(info.tensors);
    }

    bool process(FramePtr src) override {
        if (!_parser) {
            parser_init(src);
        }

        bool has_detection = false;
        auto src_cpu = src.map(AccessMode::Read);
        for (auto &tensor : src_cpu) {
            std::vector<DetectionMetadata> objects = _parser->parse(*tensor);

            if (_apply_nms) {
                perform_nms(objects);
            }

            for (auto &bbox : objects) {
                DetectionMetadata meta(src->metadata().add(DetectionMetadata::name));
                _logger->debug("bbox[{:f}, {:f}, {:f}, {:f}], {:f}", bbox.x_min(), bbox.y_min(), bbox.x_max(),
                               bbox.y_max(), bbox.confidence());
                meta.init(bbox.x_min(), bbox.y_min(), bbox.x_max(), bbox.y_max(), bbox.confidence(), bbox.label_id(),
                          get_label_by_id(bbox.label_id()));
            }
            has_detection |= !objects.empty();
        }

        if (has_detection)
            _logger->debug("--- end of detected objects ---");

        return true;
    }

  protected:
    std::shared_ptr<spdlog::logger> _logger;
    std::vector<std::string> _labels;
    YoloParserBuilder _builder;
    std::unique_ptr<YoloParser> _parser;
    double _iou_threshold = param::default_iou_threshold;
    bool _apply_nms = param::default_nms;

    const std::string &get_label_by_id(size_t label_id) const noexcept {
        static const std::string empty_label;
        if (_labels.empty())
            return empty_label;
        if (label_id >= _labels.size()) {
            try {
                _logger->warn("Label ID {} is out of range", label_id);
            } catch (...) {
                std::cerr << "Unknown exception occurred in get_label_by_id" << std::endl;
            }
            return empty_label;
        }
        return _labels[label_id];
    }

    void parser_init(FramePtr first_frame) {
        auto model_info = find_metadata<ModelInfoMetadata>(*first_frame);
        if (!model_info)
            throw std::runtime_error("Model info is not found");
        const auto &input_shape_info = model_info->input().tensors;
        const auto &input_shape = input_shape_info.front().shape;
        dlstreamer::ImageLayout image_layout(input_shape);

        _builder.set_image_info(input_shape[image_layout.w_position()], input_shape[image_layout.h_position()]);
        _parser = _builder.build();
    }

    void perform_nms(std::vector<DetectionMetadata> &candidates) {
        std::sort(candidates.rbegin(), candidates.rend(),
                  [](const DetectionMetadata &l, const DetectionMetadata &r) -> bool {
                      return l.confidence() < r.confidence();
                  });

        for (auto p_first_candidate = candidates.begin(); p_first_candidate != candidates.end(); ++p_first_candidate) {
            const auto &first_candidate = *p_first_candidate;
            double first_candidate_area = (first_candidate.x_max() - first_candidate.x_min()) *
                                          (first_candidate.y_max() - first_candidate.y_min());

            for (auto p_candidate = p_first_candidate + 1; p_candidate != candidates.end();) {
                const auto &candidate = *p_candidate;

                const double inter_width = std::min(first_candidate.x_max(), candidate.x_max()) -
                                           std::max(first_candidate.x_min(), candidate.x_min());
                const double inter_height = std::min(first_candidate.y_max(), candidate.y_max()) -
                                            std::max(first_candidate.y_min(), candidate.y_min());
                if (inter_width <= 0.0 || inter_height <= 0.0) {
                    ++p_candidate;
                    continue;
                }

                const double inter_area = inter_width * inter_height;
                const double candidate_area =
                    (candidate.x_max() - candidate.x_min()) * (candidate.y_max() - candidate.y_min());
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

extern "C" {
ElementDesc tensor_postproc_yolo = {.name = "tensor_postproc_yolo",
                                    .description = "Post-processing of YOLO models to extract bounding box list",
                                    .author = "Intel Corporation",
                                    .params = &params_desc,
                                    .input_info = MAKE_FRAME_INFO_VECTOR({MediaType::Tensors}),
                                    .output_info = MAKE_FRAME_INFO_VECTOR({MediaType::Tensors}),
                                    .create = create_element<PostProcYolo>,
                                    .flags = 0};
}

} // namespace dlstreamer
