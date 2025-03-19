/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/base/transform.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/utils.h"
#include <array>
#include <cmath>

namespace dlstreamer {
namespace overlay {
class Color {
  private:
    std::array<uint8_t, 4> vec;

  public:
    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : vec{r, g, b, a} {};
    Color(uint32_t rgba) {
        vec = {static_cast<uint8_t>(rgba), static_cast<uint8_t>(rgba >> 8), static_cast<uint8_t>(rgba >> 16),
               static_cast<uint8_t>(rgba >> 24)};
    }

    std::array<uint8_t, 4> get_array() {
        return vec;
    };
    uint32_t get_uint32(dlstreamer::Format format) {
        auto image_format = static_cast<dlstreamer::ImageFormat>(format);
        if (image_format == dlstreamer::ImageFormat::RGB || image_format == dlstreamer::ImageFormat::RGBX) {
            return static_cast<uint32_t>(vec[0]) << 0 | static_cast<uint32_t>(vec[1]) << 8 |
                   static_cast<uint32_t>(vec[2]) << 16 | static_cast<uint32_t>(vec[3]) << 24;
        } else if (image_format == dlstreamer::ImageFormat::BGR || image_format == dlstreamer::ImageFormat::BGRX) {
            return static_cast<uint32_t>(vec[0]) << 16 | static_cast<uint32_t>(vec[1]) << 8 |
                   static_cast<uint32_t>(vec[2]) << 0 | static_cast<uint32_t>(vec[3]) << 24;
        } else {
            throw std::runtime_error("Unsupported color format");
        }
    }
};

namespace prims {
struct Rect {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    uint32_t color;
    uint32_t thickness;
};
struct Text {
    std::string str;
    int32_t x;
    int32_t y;
    uint32_t color;
    uint32_t region_index;
};
struct Circle {
    int32_t x;
    int32_t y;
    uint32_t radius;
    uint32_t color;
};
struct Line {
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
    uint32_t color;
    uint32_t thickness;
    bool steep = false;
};
struct Mask {
    uint8_t *data;
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    uint32_t color;
};
} // namespace prims
} // namespace overlay

class MetaOverlayBase : public BaseTransformInplace {
  public:
    struct param {
        static constexpr auto lines_thickness = "lines-thickness";
    };
    struct dflt {
        static constexpr auto lines_thickness = 2;
    };

    MetaOverlayBase(DictionaryCPtr params, const ContextPtr &app_context)
        : BaseTransformInplace(app_context), _default_color(index_to_color(1)) {
        _lines_thickness = params->get<int>(param::lines_thickness, dflt::lines_thickness);
    }

    static ParamDescVector params_desc;

  protected:
    uint32_t _lines_thickness;
    int _font_height = 25;
    const double _radius_multiplier = 0.0025;
    static constexpr auto _label_mask_key = "label_mask";
    static constexpr auto _min_keypoints_data_dims_size = 2;

    overlay::Color _default_color;

    static void append(std::ostringstream &ss, const std::string &str) {
        if (ss.rdbuf()->in_avail())
            ss << " ";
        ss << str;
    }

    void prepare_keypoints(FramePtr frame, std::vector<overlay::prims::Circle> *keypoints,
                           std::vector<overlay::prims::Line> *lines, overlay::prims::Rect &rectangle) {
        for (auto &it : frame->metadata()) {
            InferenceResultMetadata meta(it);
            if (meta.format() != "keypoints")
                continue;
            auto tensor = meta.tensor();
            auto shape = tensor->info().shape;
            if (shape.size() < _min_keypoints_data_dims_size) {
                throw std::runtime_error("Keypoints tensor dimension " + std::to_string(shape.size()) +
                                         " is not supported (less than " +
                                         std::to_string(_min_keypoints_data_dims_size) + ").");
            }
            size_t points_num = shape[0];
            size_t point_dimension = shape[1];

            if (tensor->info().size() != points_num * point_dimension)
                throw std::logic_error("The size of the keypoints data does not match the dimension: Size=" +
                                       std::to_string(shape.size()) + " Dimension=[" + std::to_string(shape[0]) + "," +
                                       std::to_string(shape[1]) + "].");
            float *keypoints_data = tensor->data<float>();
            keypoints->reserve(keypoints->size() + points_num);
            for (size_t i = 0; i < points_num; ++i) {
                float x_real = keypoints_data[point_dimension * i];
                float y_real = keypoints_data[point_dimension * i + 1];

                // Skip if one of point is exist but not found on frame (example left hand is out of frame)
                if (x_real == -1.0f && y_real == -1.0f)
                    continue;

                int32_t x_lm = std::lround(rectangle.x + rectangle.width * x_real);
                int32_t y_lm = std::lround(rectangle.y + rectangle.height * y_real);
                uint32_t radius = 1 + std::lround(_radius_multiplier * (rectangle.width + rectangle.height));
                auto color = index_to_color(i).get_uint32(_info.format);
                keypoints->emplace_back(overlay::prims::Circle{x_lm, y_lm, radius, color});
            }

            if (lines)
                prepare_keypoins_connections(it, keypoints_data, shape, rectangle, lines);
        }
    }

    void prepare_keypoins_connections(DictionaryPtr meta_ptr, float *keypoints_data,
                                      std::vector<size_t> keypoints_shape, overlay::prims::Rect &rectangle,
                                      std::vector<overlay::prims::Line> *lines) {
        auto point_names = meta_ptr->get("point_names", std::vector<std::string>());
        auto point_connections = meta_ptr->get("point_connections", std::vector<std::string>());
        if (point_names.empty() || point_connections.empty())
            return;
        if (point_names.size() != keypoints_shape[0])
            throw std::logic_error("Number of point names must be equal to number of keypoints.");
        size_t point_dimension = keypoints_shape[1];
        if (point_connections.size() % 2 != 0)
            throw std::logic_error("Expected even amount of point connections.");
        lines->reserve(lines->size() + point_connections.size() / 2);
        auto default_color = _default_color.get_uint32(_info.format);
        for (size_t i = 0; i < point_connections.size(); i += 2) {
            auto point_name_1 = point_connections[i];
            auto point_name_2 = point_connections[i + 1];

            auto index_1_it = std::find(point_names.begin(), point_names.end(), point_name_1);
            auto index_2_it = std::find(point_names.begin(), point_names.end(), point_name_2);

            if (index_1_it == point_names.end())
                throw std::runtime_error("Point name \"" + std::string(point_name_1) +
                                         "\" has not been found in point connections.");

            if (index_2_it == point_names.end())
                throw std::runtime_error("Point name \"" + std::string(point_name_2) +
                                         "\" has not been found in point connections.");

            if (index_1_it == index_2_it)
                throw std::logic_error("Point names in connection are the same: " + std::string(point_name_1) + " / " +
                                       std::string(point_name_2));

            size_t index_1 = std::distance(point_names.begin(), index_1_it);
            size_t index_2 = std::distance(point_names.begin(), index_2_it);

            index_1 = std::lround(point_dimension * index_1);
            index_2 = std::lround(point_dimension * index_2);

            float x1_real = keypoints_data[index_1];
            float y1_real = keypoints_data[index_1 + 1];
            float x2_real = keypoints_data[index_2];
            float y2_real = keypoints_data[index_2 + 1];

            // Skip if one of point is exist but not found on frame (example left hand is out of frame)
            if ((x1_real == -1.0f && y1_real == -1.0f) or (x2_real == -1.0f && y2_real == -1.0f))
                continue;

            int x1 = std::lround(rectangle.x + rectangle.width * x1_real);
            int y1 = std::lround(rectangle.y + rectangle.height * y1_real);
            int x2 = std::lround(rectangle.x + rectangle.width * x2_real);
            int y2 = std::lround(rectangle.y + rectangle.height * y2_real);
            lines->emplace_back(overlay::prims::Line{x1, y1, x2, y2, default_color, _lines_thickness});
        }
    }

    void prepare_prims(FramePtr frame, std::vector<FramePtr> &regions, std::vector<overlay::prims::Rect> *rects,
                       std::vector<overlay::prims::Text> *texts, std::vector<overlay::prims::Mask> *masks,
                       std::vector<overlay::prims::Circle> *keypoints, std::vector<overlay::prims::Line> *lines) {
        ImageInfo image_info(frame->tensor(0)->info());
        for (size_t i = 0; i < regions.size(); i++) {
            auto &region = regions[i];
            auto region_tensor = region->tensor(0);

            ImageInfo region_info(region_tensor->info());
            int offset_x = 0, offset_y = 0;
            auto detection_meta = find_metadata<DetectionMetadata>(*region);
            if (detection_meta) {
                offset_x = std::lround(detection_meta->x_min() * image_info.width());
                offset_y = std::lround(detection_meta->y_min() * image_info.height());
            }

            uint32_t color = _default_color.get_uint32(_info.format);
            auto object_id_meta = find_metadata<ObjectIdMetadata>(*region);
            if (object_id_meta)
                color = index_to_color(object_id_meta->id()).get_uint32(_info.format);

            std::string label;
            if (texts) {
                std::ostringstream ss;
                if (object_id_meta)
                    ss << object_id_meta->id() << ":";
                for (auto &meta : region->metadata()) {
                    auto meta_label = meta->try_get("label");
                    if (meta_label)
                        append(ss, any_cast<std::string>(*meta_label));
                }
                label = ss.str();
                if (!label.empty()) {
                    overlay::prims::Text text;
                    text.str = ss.str();
                    text.x = offset_x;
                    text.y = (offset_y < _font_height) ? (offset_y + _font_height) : offset_y; // TODO text location
                    text.color = color;
                    text.region_index = i;
                    texts->emplace_back(text);
                }
            }

            auto label_mask_meta = find_metadata(*regions[i], _label_mask_key);
            if (masks && label_mask_meta) {
                overlay::prims::Mask mask;
                auto label_mask = InferenceResultMetadata(label_mask_meta).tensor();
                ImageInfo text_info(label_mask->info());
                mask.data = label_mask->data<uint8_t>();
                mask.w = text_info.width();
                mask.h = text_info.height();
                mask.x = offset_x;
                mask.y = (offset_y > mask.h) ? (offset_y - mask.h) : offset_y; // TODO text location
                mask.color = color;
                masks->emplace_back(mask);
            }

            if (rects) {
                overlay::prims::Rect rect;
                rect.x = offset_x;
                rect.y = offset_y;
                rect.width = region_info.width();
                rect.height = region_info.height();
                rect.thickness = _lines_thickness;
                rect.color = color;
                if (rect.x + rect.width + 2 * rect.thickness > image_info.width())
                    rect.width = image_info.width() - rect.x - 2 * rect.thickness;
                if (rect.y + rect.height + 2 * rect.thickness > image_info.height())
                    rect.height = image_info.height() - rect.y - 2 * rect.thickness;
                // draw full-frame rectangle only if label or label_mask found
                if (region != frame || !label.empty() || label_mask_meta)
                    rects->emplace_back(rect);

                if (keypoints && lines)
                    prepare_keypoints(region, keypoints, lines, rect);
            }
        }
    }

    overlay::Color index_to_color(size_t index) {
        static std::vector<overlay::Color> color_table{
            {overlay::Color(255, 0, 0), overlay::Color(0, 255, 0), overlay::Color(0, 0, 255),
             overlay::Color(255, 255, 0), overlay::Color(0, 255, 255), overlay::Color(255, 0, 255),
             overlay::Color(255, 170, 0), overlay::Color(255, 0, 170), overlay::Color(0, 255, 170),
             overlay::Color(170, 255, 0), overlay::Color(170, 0, 255), overlay::Color(0, 170, 255),
             overlay::Color(255, 85, 0), overlay::Color(85, 255, 0), overlay::Color(0, 255, 85),
             overlay::Color(0, 85, 255), overlay::Color(85, 0, 255), overlay::Color(255, 0, 85)}};
        return color_table[index % color_table.size()];
    }
};

ParamDescVector MetaOverlayBase::params_desc = {
    {param::lines_thickness, "Thickness of lines and rectangles", dflt::lines_thickness},
};

} // namespace dlstreamer
