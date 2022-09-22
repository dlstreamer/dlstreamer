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

class BaseWatermark : public BaseTransformInplace {
  public:
    struct param {
        static constexpr auto lines_thickness = "lines-thickness";
    };
    struct dflt {
        static constexpr auto lines_thickness = 2;
    };

    BaseWatermark(DictionaryCPtr params, const ContextPtr &app_context) : BaseTransformInplace(app_context) {
        _lines_thickness = params->get<int>(param::lines_thickness, dflt::lines_thickness);
    }

    static ParamDescVector params_desc;

  protected:
    int _lines_thickness;
    int _font_height = 25;
    static constexpr auto _label_mask_key = "label_mask";

    struct RectPrim {
        int32_t x;
        int32_t y;
        int32_t width;
        int32_t height;
        uint32_t color;
        uint32_t thickness;
    };
    struct TextPrim {
        std::string str;
        int32_t x;
        int32_t y;
        uint32_t color;
        uint32_t region_index;
    };
    struct MaskPrim {
        uint8_t *data;
        int32_t x;
        int32_t y;
        int32_t w;
        int32_t h;
        uint32_t color;
    };

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
        uint32_t get_uint32(ImageFormat format) {
            if (format == ImageFormat::RGB || format == ImageFormat::RGBX) {
                return static_cast<uint32_t>(vec[0]) << 0 | static_cast<uint32_t>(vec[1]) << 8 |
                       static_cast<uint32_t>(vec[2]) << 16 | static_cast<uint32_t>(vec[3]) << 24;
            } else if (format == ImageFormat::BGR || format == ImageFormat::BGRX) {
                return static_cast<uint32_t>(vec[0]) << 16 | static_cast<uint32_t>(vec[1]) << 8 |
                       static_cast<uint32_t>(vec[2]) << 0 | static_cast<uint32_t>(vec[3]) << 24;
            } else {
                throw std::runtime_error("Unsupported color format");
            }
        }
    };

    static void append(std::ostringstream &ss, const std::string &str) {
        if (ss.rdbuf()->in_avail())
            ss << " ";
        ss << str;
    }

    void prepare_prims(FramePtr frame, std::vector<FramePtr> &regions, RectPrim *rects, size_t &num_rects,
                       TextPrim *texts, size_t &num_texts, MaskPrim *masks, size_t &num_masks) {
        ImageInfo image_info(frame->tensor(0)->info());
        uint32_t default_color = index_to_color(1).get_uint32(static_cast<ImageFormat>(_info.format));

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

            uint32_t color = default_color;
            auto object_id_meta = find_metadata<ObjectIdMetadata>(*region);
            if (object_id_meta)
                color = index_to_color(object_id_meta->id()).get_uint32(static_cast<ImageFormat>(_info.format));

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
                    TextPrim &text = texts[num_texts++];
                    text.str = ss.str();
                    text.x = offset_x;
                    text.y = (offset_y < _font_height) ? (offset_y + _font_height) : offset_y; // TODO text location
                    text.color = color;
                    text.region_index = i;
                }
            }

            auto label_mask_meta = find_metadata(*regions[i], _label_mask_key);
            if (masks && label_mask_meta) {
                MaskPrim &mask = masks[num_masks++];
                auto label_mask = InferenceResultMetadata(label_mask_meta).tensor();
                ImageInfo text_info(label_mask->info());
                mask.data = label_mask->data<uint8_t>();
                mask.w = text_info.width();
                mask.h = text_info.height();
                mask.x = offset_x;
                mask.y = (offset_y > mask.h) ? (offset_y - mask.h) : offset_y; // TODO text location
                mask.color = color;
            }

            // draw full-frame rectangle only if label or label_mask found
            if (rects && (region != frame || !label.empty() || label_mask_meta)) {
                RectPrim &rect = rects[num_rects++];
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
            }
        }
    }

    BaseWatermark::Color index_to_color(size_t index) {
        static std::vector<Color> color_table{
            {Color(255, 0, 0), Color(0, 255, 0), Color(0, 0, 255), Color(255, 255, 0), Color(0, 255, 255),
             Color(255, 0, 255), Color(255, 170, 0), Color(255, 0, 170), Color(0, 255, 170), Color(170, 255, 0),
             Color(170, 0, 255), Color(0, 170, 255), Color(255, 85, 0), Color(85, 255, 0), Color(0, 255, 85),
             Color(0, 85, 255), Color(85, 0, 255), Color(255, 0, 85)}};
        return color_table[index % color_table.size()];
    }
};

ParamDescVector BaseWatermark::params_desc = {
    {param::lines_thickness, "Thickness of lines and rectangles", dflt::lines_thickness},
};

} // namespace dlstreamer
