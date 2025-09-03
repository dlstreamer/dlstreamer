/*******************************************************************************
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/opencv/elements/opencv_barcode_detector.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/cpu/context.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/opencv/context.h"
#include "dlstreamer_logger.h"
#include <opencv2/imgproc.hpp>
#if (CV_VERSION_MAJOR > 4 || (CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR > 7))
#include <opencv2/objdetect/barcode.hpp>
#else
#include <opencv2/barcode.hpp>
#endif
#include <sstream>
#include <string>

namespace dlstreamer {
namespace param {
static constexpr auto allow_undecoded = "allow_undecoded";
static constexpr auto undecoded_label = "undecoded_label";
static constexpr auto add_type = "add_type";

static constexpr auto default_undecoded_label = "<undecodable>";
}; // namespace param

static ParamDescVector params_desc = {
    {param::allow_undecoded, "Allow undecoded barcodes to be added as ROI", false},
    {param::add_type, "Adds Barcode type to the label", false},
    {param::undecoded_label, "Label for undecoded barcodes", param::default_undecoded_label},
};

class OpencvBarcodeDetector : public BaseTransformInplace {
  public:
    OpencvBarcodeDetector(DictionaryCPtr params, const ContextPtr &app_context)
        : BaseTransformInplace(app_context),
          _logger(log::get_or_nullsink(params->get(param::logger_name, std::string()))) {
        _allow_undecoded = params->get<bool>(param::allow_undecoded, false);
        _add_barcode_type = params->get<bool>(param::add_type, false);
        _undecoded_label = params->get<std::string>(param::undecoded_label, param::default_undecoded_label);
    }

    bool init_once() override {
        auto cpu_context = std::make_shared<CPUContext>();
        auto opencv_context = std::make_shared<OpenCVContext>();
        _opencv_mapper = create_mapper({_app_context, cpu_context, opencv_context});
        _bardet = cv::makePtr<cv::barcode::BarcodeDetector>();
        return true;
    }

    bool process(FramePtr frame) override {

        if (frame->num_tensors() < 1) {
            SPDLOG_LOGGER_ERROR(_logger, "No Inference found!");
            return false;
        }

        auto cv_tensor = ptr_cast<OpenCVTensor>(_opencv_mapper->map(frame->tensor(0), AccessMode::Read));
        cv::Mat cv_mat = *cv_tensor;
        const ImageInfo &frame_info = frame->tensor(0)->info();
        for (auto &region : frame->regions()) {
            auto detection_meta = find_metadata<DetectionMetadata>(*region);
            if (!detection_meta) {
                continue;
            }
            auto x = std::lround(detection_meta->x_min() * static_cast<double>(frame_info.width()));
            auto y = std::lround(detection_meta->y_min() * static_cast<double>(frame_info.height()));
            auto w = std::lround(detection_meta->x_max() * static_cast<double>(frame_info.width())) - x;
            auto h = std::lround(detection_meta->y_max() * static_cast<double>(frame_info.height())) - y;
            cv::Rect rect(x, y, w, h);
            cv::Mat croppedImage = cv_mat(rect);
            std::vector<cv::String> decode_info;
#if (CV_VERSION_MAJOR > 4 || (CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR > 7))
            std::vector<cv::String> decoded_type;
#else
            std::vector<cv::barcode::BarcodeType> decoded_type;
#endif
            std::vector<cv::Point> corners;
            bool result_detection;
            try {
#if (CV_VERSION_MAJOR > 4 || (CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR > 7))
                result_detection = _bardet->detectAndDecodeWithType(croppedImage, decode_info, decoded_type, corners);
#else
                result_detection = _bardet->detectAndDecode(croppedImage, decode_info, decoded_type, corners);
#endif
            } catch (const std::exception &e) {
                SPDLOG_LOGGER_ERROR(_logger, "Exception during Barcode Detection: {}", e.what());
                return false;
            }
            if (!result_detection || corners.empty()) {
                continue;
            }
            for (size_t i = 0; i < corners.size(); i += 4) {
                size_t bar_idx = i / 4;
                const bool decoded = bar_idx < decode_info.size() && !decode_info[bar_idx].empty();
                if (!decoded && !_allow_undecoded)
                    continue;
                DetectionMetadata dmeta(frame->metadata().add(DetectionMetadata::name));

                std::string label;
                cv::Rect box = cv::boundingRect(corners);
                if (decoded) {
                    std::ostringstream oss;
                    if (_add_barcode_type && bar_idx < decoded_type.size())
                        oss << '[' << decoded_type[bar_idx] << ']';
                    oss << decode_info[bar_idx];
                    label = oss.str();
                } else {
                    label = _undecoded_label;
                }
                dmeta.init((x + box.x) / static_cast<double>(frame_info.width()),
                           (y + box.y) / static_cast<double>(frame_info.height()),
                           (x + box.br().x) / static_cast<double>(frame_info.width()),
                           (y + box.br().y) / static_cast<double>(frame_info.height()), 1.0, -1, label);
            }
        }
        return true;
    }

  private:
    cv::Ptr<cv::barcode::BarcodeDetector> _bardet;
    MemoryMapperPtr _opencv_mapper;
    std::shared_ptr<spdlog::logger> _logger;
    bool _allow_undecoded;
    bool _add_barcode_type;
    std::string _undecoded_label;
};

extern "C" {
ElementDesc opencv_barcode_detector = {.name = "opencv_barcode_detector",
                                       .description = "Detect Barcodes using openCV",
                                       .author = "Intel Corporation",
                                       .params = &params_desc,
                                       .input_info = MAKE_FRAME_INFO_VECTOR({MediaType::Image}),
                                       .output_info = MAKE_FRAME_INFO_VECTOR({MediaType::Image}),
                                       .create = create_element<OpencvBarcodeDetector>,
                                       .flags = 0};
}

} // namespace dlstreamer
