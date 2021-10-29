/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "opencv_preproc.hpp"

#include <opencv_utils/opencv_utils.h>

#include <inference_backend/pre_proc.h>

#include <frame_data.hpp>
#include <safe_arithmetic.hpp>

using namespace InferenceBackend;

namespace {
int format_planes_num(InputImageLayerDesc::ColorSpace color_format) {
    switch (color_format) {
    case InputImageLayerDesc::ColorSpace::YUV:
        // TODO: YUV is not implemented yet. Refactor later for supported YUV
        throw std::runtime_error("Unsupported YUV color space format");
    case InputImageLayerDesc::ColorSpace::BGR:
    case InputImageLayerDesc::ColorSpace::RGB:
        return 3;
    case InputImageLayerDesc::ColorSpace::GRAYSCALE:
        return 1;
    case InputImageLayerDesc::ColorSpace::NO:
    default:
        return 0;
    }
}
} // namespace

OpenCVPreProc::OpenCVPreProc(GstVideoInfo *input_video_info, const TensorCaps &output_tensor_info,
                             const InputImageLayerDesc::Ptr &pre_proc_info)
    : _input_video_info(input_video_info), _output_tensor_info(output_tensor_info), _pre_proc_info(pre_proc_info) {
    if (!_input_video_info)
        throw std::invalid_argument("OpenCVPreProc: GstVideoInfo is null");
}

void OpenCVPreProc::process(GstBuffer *in_buffer, GstBuffer *out_buffer) {
    if (!in_buffer || !out_buffer)
        throw std::invalid_argument("OpenCVPreProc: GstBuffer is null");

    FrameData src;
    src.Map(in_buffer, _input_video_info, InferenceBackend::MemoryType::SYSTEM, GST_MAP_READ);

    /* TODO: hardcoded RGB fallback */
    auto target_color = _pre_proc_info && _pre_proc_info->getTargetColorSpace() != InputImageLayerDesc::ColorSpace::NO
                            ? _pre_proc_info->getTargetColorSpace()
                            : InputImageLayerDesc::ColorSpace::RGB;
    FrameData dst;
    dst.Map(out_buffer, _output_tensor_info, GST_MAP_WRITE, InferenceBackend::MemoryType::SYSTEM,
            format_planes_num(target_color));

    auto frame_data_to_image = [](const FrameData &frame_data) {
        Image image;
        image.type = frame_data.GetMemoryType();
        for (auto i = 0u; i < frame_data.GetPlanesNum(); i++) {
            image.planes[i] = frame_data.GetPlane(i);
            image.stride[i] = frame_data.GetStride(i);
            image.offsets[i] = frame_data.GetOffset(i);
        }
        image.format = frame_data.GetFormat();
        image.width = frame_data.GetWidth();
        image.height = frame_data.GetHeight();
        image.size = frame_data.GetSize();
        image.rect = {0, 0, image.width, image.height};
        return image;
    };

    auto vpp = std::unique_ptr<ImagePreprocessor>(ImagePreprocessor::Create(ImagePreprocessorType::OPENCV));
    auto src_image = frame_data_to_image(src);
    auto dst_image = frame_data_to_image(dst);

    vpp->Convert(src_image, dst_image, _pre_proc_info, nullptr, false);
}

void OpenCVPreProc::process(GstBuffer *) {
    throw std::runtime_error("OpenCVPreProc: In-place processing is not supported");
}

size_t OpenCVPreProc::output_size() const {
    auto out_area = safe_mul(_output_tensor_info.GetWidth(), _output_tensor_info.GetHeight());
    auto format = gst_format_to_four_CC(GST_VIDEO_INFO_FORMAT(_input_video_info));

    size_t out_size = 0;
    if (_pre_proc_info && _pre_proc_info->doNeedColorSpaceConversion(format)) {
        switch (_pre_proc_info->getTargetColorSpace()) {
        case InputImageLayerDesc::ColorSpace::RGB:
        case InputImageLayerDesc::ColorSpace::BGR:
            out_size = 3 * out_area;
            break;
        case InputImageLayerDesc::ColorSpace::GRAYSCALE:
            out_size = out_area;
            break;
        case InputImageLayerDesc::ColorSpace::YUV:
            out_size = 3 * out_area / 2;
            break;
        default:
            break;
        }
    } else {
        // TODO: hardcoded fallback to RGB
        out_size = 3 * out_area;
    }

    return out_size;
}

bool OpenCVPreProc::need_preprocessing() const {
    if (_pre_proc_info && _pre_proc_info->isDefined())
        return true;
    /* TODO: color space */
    return static_cast<size_t>(_input_video_info->width) != _output_tensor_info.GetWidth() ||
           static_cast<size_t>(_input_video_info->height) != _output_tensor_info.GetHeight() ||
           GST_VIDEO_INFO_FORMAT(_input_video_info) != GST_VIDEO_FORMAT_RGBx; // TODO: what are possible formats?
}
