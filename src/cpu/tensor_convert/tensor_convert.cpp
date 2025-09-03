/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/cpu/elements/tensor_convert.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer_logger.h"

namespace dlstreamer {

class TensorConvert : public BaseTransform {
  public:
    TensorConvert(DictionaryCPtr params, const ContextPtr &app_context)
        : BaseTransform(app_context), _logger(log::get_or_nullsink(params->get(param::logger_name, std::string()))) {
    }

    FrameInfoVector get_input_info() override {
        if (_output_info.tensors.empty())
            return tensor_convert.input_info();
        if (_output_info.tensors[0].dtype != DataType::UInt8)
            return {};
        if (ImageInfo(_output_info.tensors[0]).layout() == ImageLayout::Any) {
            return tensor_convert.input_info();
        } else {
            auto image_format_vector = tensor_info_to_image_format_vector(_output_info.tensors[0]);
            FrameInfoVector ret;
            for (auto video_format : image_format_vector) {
                FrameInfo info = _output_info;
                info.media_type = MediaType::Image;
                info.format = static_cast<int>(video_format);
                ret.push_back(info);
            }
            return ret;
        }
    }

    FrameInfoVector get_output_info() override {
        if (_input_info.tensors.empty()) {
            return tensor_convert.output_info();
        } else {
            FrameInfo info = _input_info;
            info.media_type = MediaType::Tensors;

            // add batch dimension with batch=1
            FrameInfo info_with_batch = info;
            for (auto &tinfo : info_with_batch.tensors) {
                if ((tinfo.shape.size() < 2) || (tinfo.stride.size() < 2))
                    throw std::logic_error("Invalid tensor info received in 'tensor_convert' element");

                tinfo.shape.insert(tinfo.shape.begin(), 1); // TODO: remove batch hardcode?
                // add to contiguous stride
                tinfo.stride.insert(tinfo.stride.begin(), tinfo.shape[1] * tinfo.stride[0]);
            }

            return {info, info_with_batch};
        }
    }

    FramePtr process(FramePtr src) override {
        return src;
    }

    bool process(FramePtr /*src*/, FramePtr /*dst*/) override {
        throw std::runtime_error("Unsupported");
    }

    std::function<FramePtr()> get_output_allocator() override {
        return nullptr;
    }

  private:
    std::shared_ptr<spdlog::logger> _logger;

    static std::vector<ImageFormat> tensor_info_to_image_format_vector(const TensorInfo &info) {
        ImageInfo image_info(info);
        switch (image_info.layout()) {
        case ImageLayout::HWC:
        case ImageLayout::NHWC:
            if (image_info.channels() == 3)
                return {ImageFormat::BGR, ImageFormat::RGB};
            else if (image_info.channels() == 4)
                return {ImageFormat::BGRX, ImageFormat::RGBX};
            else
                throw std::runtime_error("Expect number channels either 3 or 4");
        case ImageLayout::CHW:
        case ImageLayout::NCHW:
            return {ImageFormat::RGBP, ImageFormat::BGRP};
        default:
            throw std::runtime_error("Unsupported layout");
        }
    }
};

extern "C" {
ElementDesc tensor_convert = {
    .name = "tensor_convert",
    .description = "Convert (zero-copy if possible) between video/audio and tensors media type",
    .author = "Intel Corporation",
    .params = nullptr,
    .input_info = MAKE_FRAME_INFO_VECTOR({
        FrameInfo(ImageFormat::RGB, MemoryType::Any),
        FrameInfo(ImageFormat::BGR, MemoryType::Any),
        FrameInfo(ImageFormat::RGBX, MemoryType::Any),
        FrameInfo(ImageFormat::BGRX, MemoryType::Any),
        FrameInfo(ImageFormat::RGBP, MemoryType::Any),
        FrameInfo(ImageFormat::BGRP, MemoryType::Any),
    }),
    .output_info = MAKE_FRAME_INFO_VECTOR({FrameInfo(MediaType::Tensors, MemoryType::Any, {{{}, DataType::UInt8}})}),
    .create = create_element<TensorConvert>,
    .flags = 0};
}

} // namespace dlstreamer
