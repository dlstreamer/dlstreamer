/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/opencv/elements/opencv_meta_overlay.h"

#include "base_watermark.h"
#include "dlstreamer/cpu/frame_alloc.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/opencv/context.h"

namespace dlstreamer {

class OpencvMetaOverlay : public BaseWatermark {
  public:
    struct param {
        static constexpr auto font_thickness = "font-thickness";
        static constexpr auto font_scale = "font-scale";
        static constexpr auto attach_label_mask = "attach-label-mask";
    };
    struct dflt {
        static constexpr auto font_thickness = 1;
        static constexpr auto font_scale = 1.0;
        static constexpr auto attach_label_mask = false;
    };

    static ParamDescVector params_desc;

    OpencvMetaOverlay(DictionaryCPtr params, const ContextPtr &app_context) : BaseWatermark(params, app_context) {
        _font_thickness = params->get(param::font_thickness, dflt::font_thickness);
        _font_scale = params->get(param::font_scale, dflt::font_scale);
        _attach_label_mask = params->get(param::attach_label_mask, dflt::attach_label_mask);

        int baseline = 0;
        _font_height = cv::getTextSize(" ", _font_face, _font_scale, _font_thickness, &baseline).height;
    }

    bool init_once() override {
        if (_info.memory_type == MemoryType::CPU) {
            auto cpu_context = std::make_shared<CPUContext>();
            auto opencv_context = std::make_shared<OpenCVContext>();
            _opencv_mapper = create_mapper({_app_context, cpu_context, opencv_context});
        }
        return true;
    }

    bool process(FramePtr frame) override {
        // All regions plus full frame
        std::vector<FramePtr> regions = frame->regions();
        regions.push_back(frame);

        size_t num_rects = 0, num_texts = 0, num_masks = 0;

        if (_attach_label_mask) {
            std::vector<TextPrim> texts(regions.size());
            prepare_prims(frame, regions, NULL, num_rects, texts.data(), num_texts, NULL, num_masks);
            for (size_t i = 0; i < num_texts; i++) {
                auto &text = texts[i];

                int baseline = 0;
                auto size = cv::getTextSize(text.str, _font_face, _font_scale, _font_thickness, &baseline);
                cv::Mat m = cv::Mat::zeros(size.height + baseline, size.width, CV_8UC1);
                cv::putText(m, text.str, {0, size.height}, _font_face, _font_scale, 255, _font_thickness, _line_type);
                // attach to region
                auto meta = add_metadata<InferenceResultMetadata>(*regions[text.region_index], _label_mask_key);
                meta.init_tensor_data(OpenCVTensor(m), "", _label_mask_key);
            }
            return true;
        }

        // prepare primitives
        std::vector<RectPrim> rects(regions.size());
        std::vector<TextPrim> texts(regions.size());
        prepare_prims(frame, regions, rects.data(), num_rects, texts.data(), num_texts, NULL, num_masks);

        // map to OpenCV
        DLS_CHECK(_opencv_mapper)
        cv::Mat mat = *ptr_cast<OpenCVTensor>(_opencv_mapper->map(frame->tensor(), AccessMode::Write));

        // render
        for (size_t i = 0; i < num_rects; i++) {
            auto &rect = rects[i];
            cv::rectangle(mat, {rect.x, rect.y}, {rect.x + rect.width, rect.y + rect.height}, color_to_cv(rect.color),
                          rect.thickness, _line_type);
        }
        for (size_t i = 0; i < num_texts; i++) {
            auto &text = texts[i];
            cv::putText(mat, text.str, {text.x, text.y}, _font_face, _font_scale, color_to_cv(text.color),
                        _font_thickness, _line_type);
        }

        return true;
    }

  private:
    MemoryMapperPtr _opencv_mapper;
    bool _attach_label_mask;
    int _line_type = cv::LINE_8; // cv::LINE_AA;
    cv::HersheyFonts _font_face = cv::FONT_HERSHEY_TRIPLEX;
    double _font_scale;
    int _font_thickness;

    void append(std::ostringstream &ss, const std::string &str) {
        if (!ss.str().empty())
            ss << " ";
        ss << str;
    }

    static cv::Scalar color_to_cv(uint32_t color) {
        auto c = Color(color).get_array();
        return {double(c[0]), double(c[1]), double(c[2])};
    }
};

ParamDescVector OpencvMetaOverlay::params_desc = {
    {BaseWatermark::param::lines_thickness, "Thickness of lines and rectangles", BaseWatermark::dflt::lines_thickness},
    {param::font_thickness, "Font thickness", dflt::font_thickness},
    {param::font_scale, "Font scale", dflt::font_scale},
    {param::attach_label_mask, "Attach label mask as metadata, image not changed", false},
};

extern "C" {
ElementDesc opencv_meta_overlay = {
    .name = "opencv_meta_overlay",
    .description = "Visualize inference results using OpenCV",
    .author = "Intel Corporation",
    .params = &OpencvMetaOverlay::params_desc,
    .input_info = {FrameInfo(ImageFormat::BGRX, MemoryType::VAAPI), FrameInfo(ImageFormat::RGBX, MemoryType::VAAPI),
                   FrameInfo(ImageFormat::BGRX, MemoryType::CPU), FrameInfo(ImageFormat::RGBX, MemoryType::CPU)},
    .output_info = {FrameInfo(ImageFormat::BGRX, MemoryType::VAAPI), FrameInfo(ImageFormat::RGBX, MemoryType::VAAPI),
                    FrameInfo(ImageFormat::BGRX, MemoryType::CPU), FrameInfo(ImageFormat::RGBX, MemoryType::CPU)},
    .create = create_element<OpencvMetaOverlay>,
    .flags = 0};
}

} // namespace dlstreamer
