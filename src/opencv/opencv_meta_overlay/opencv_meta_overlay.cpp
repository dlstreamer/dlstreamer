/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/opencv/elements/opencv_meta_overlay.h"

#include "base_meta_overlay.h"
#include "dlstreamer/cpu/frame_alloc.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/opencv/context.h"

namespace dlstreamer {

class OpencvMetaOverlay : public MetaOverlayBase {
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

    OpencvMetaOverlay(DictionaryCPtr params, const ContextPtr &app_context) : MetaOverlayBase(params, app_context) {
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

        if (_attach_label_mask) {
            std::vector<overlay::prims::Text> texts;
            texts.reserve(regions.size());
            prepare_prims(frame, regions, nullptr, &texts, nullptr, nullptr, nullptr);
            for (auto &text : texts) {
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
        std::vector<overlay::prims::Rect> rects;
        std::vector<overlay::prims::Text> texts;
        std::vector<overlay::prims::Circle> keypoints;
        std::vector<overlay::prims::Line> lines;
        rects.reserve(regions.size());
        texts.reserve(regions.size());
        prepare_prims(frame, regions, &rects, &texts, nullptr, &keypoints, &lines);

        // map to OpenCV
        DLS_CHECK(_opencv_mapper)
        FramePtr mapped_frame = _opencv_mapper->map(frame, AccessMode::ReadWrite);
        cv::Mat mat = *ptr_cast<OpenCVTensor>(mapped_frame->tensor());

        // render
        for (auto &rect : rects) {
            cv::rectangle(mat, {rect.x, rect.y}, {rect.x + rect.width, rect.y + rect.height}, color_to_cv(rect.color),
                          rect.thickness, _line_type);
        }
        for (auto &text : texts) {
            cv::putText(mat, text.str, {text.x, text.y}, _font_face, _font_scale, color_to_cv(text.color),
                        _font_thickness, _line_type);
        }
        for (auto &circle : keypoints) {
            cv::circle(mat, {circle.x, circle.y}, circle.radius, color_to_cv(circle.color), cv::FILLED);
        }
        for (auto &line : lines) {
            cv::line(mat, {line.x1, line.y1}, {line.x2, line.y2}, color_to_cv(line.color), line.thickness);
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
        auto c = overlay::Color(color).get_array();
        return {double(c[0]), double(c[1]), double(c[2])};
    }
};

ParamDescVector OpencvMetaOverlay::params_desc = {
    {MetaOverlayBase::param::lines_thickness, "Thickness of lines and rectangles",
     MetaOverlayBase::dflt::lines_thickness},
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
    .input_info = MAKE_FRAME_INFO_VECTOR(
        {FrameInfo(ImageFormat::BGRX, MemoryType::VA), FrameInfo(ImageFormat::RGBX, MemoryType::VA),
         FrameInfo(ImageFormat::BGRX, MemoryType::VAAPI), FrameInfo(ImageFormat::RGBX, MemoryType::VAAPI),
         FrameInfo(ImageFormat::BGRX, MemoryType::CPU), FrameInfo(ImageFormat::RGBX, MemoryType::CPU)}),
    .output_info = MAKE_FRAME_INFO_VECTOR(
        {FrameInfo(ImageFormat::BGRX, MemoryType::VA), FrameInfo(ImageFormat::RGBX, MemoryType::VA),
         FrameInfo(ImageFormat::BGRX, MemoryType::VAAPI), FrameInfo(ImageFormat::RGBX, MemoryType::VAAPI),
         FrameInfo(ImageFormat::BGRX, MemoryType::CPU), FrameInfo(ImageFormat::RGBX, MemoryType::CPU)}),
    .create = create_element<OpencvMetaOverlay>,
    .flags = 0};
}

} // namespace dlstreamer
