/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tensor_convert.h"

namespace dlstreamer {

class TensorConvert : public TransformInplace {
  public:
    using TransformInplace::TransformInplace;

    BufferInfoVector get_input_info(const BufferInfo &output_info) override {
        if (output_info.planes.empty())
            return TensorConvertDesc.input_info;
        if (output_info.planes[0].type != DataType::U8)
            return {};
        if (output_info.planes[0].layout == Layout::ANY)
            return TensorConvertDesc.input_info;
        else {
            auto fourcc_vector = plane_info_to_fourcc_vector(output_info.planes[0]);
            BufferInfoVector ret;
            for (auto fourcc : fourcc_vector) {
                BufferInfo info = output_info;
                info.media_type = MediaType::VIDEO;
                info.format = fourcc;
                ret.push_back(info);
            }
            return ret;
        }
    }

    BufferInfoVector get_output_info(const BufferInfo &input_info) override {
        if (input_info.planes.empty()) {
            return TensorConvertDesc.output_info;
        } else {
            BufferInfo info = input_info;
            info.media_type = MediaType::TENSORS;

            // add batch dimension with batch=1
            BufferInfo info_with_batch = info;
            for (auto &plane : info_with_batch.planes) {
                plane.shape.insert(plane.shape.begin(), 1);
                plane.stride.insert(plane.stride.begin(), *plane.stride.begin());
            }

            return {info, info_with_batch};
        }
    }

    void set_info(const BufferInfo &input_info, const BufferInfo &output_info) override {
        if (input_info.planes[0].type != DataType::U8 || output_info.planes[0].type != DataType::U8)
            throw std::runtime_error("Unsupported data type");
    }

    bool process(BufferPtr /*src*/) override {
        return true;
    }

  private:
    static std::vector<FourCC> plane_info_to_fourcc_vector(const PlaneInfo &info) {
        switch (info.layout) {
        case Layout::HWC:
        case Layout::NHWC:
            if (info.channels() == 3)
                return {FourCC::FOURCC_BGR, FourCC::FOURCC_RGB};
            else if (info.channels() == 4)
                return {FourCC::FOURCC_BGRX, FourCC::FOURCC_RGBX};
            else
                throw std::runtime_error("Expect number channels either 3 or 4");
        case Layout::CHW:
        case Layout::NCHW:
            return {FourCC::FOURCC_RGBP}; // TODO RGB vs BGR plane order
        default:
            throw std::runtime_error("Unsupported layout");
        }
    }
};

TransformDesc TensorConvertDesc = {
    .name = "tensor_convert",
    .description = "Convert (zero-copy if possible) between video/audio and tensors media type",
    .author = "Intel Corporation",
    .params = nullptr,
    .input_info = {BufferInfo(FourCC::FOURCC_RGB, BufferType::CPU), BufferInfo(FourCC::FOURCC_BGR, BufferType::CPU),
                   BufferInfo(FourCC::FOURCC_RGBX, BufferType::CPU), BufferInfo(FourCC::FOURCC_BGRX, BufferType::CPU),
                   BufferInfo(FourCC::FOURCC_RGBP, BufferType::CPU)},
    .output_info = {BufferInfo(MediaType::TENSORS, BufferType::CPU, {{{}, DataType::U8}})},
    .create = TransformBase::create<TensorConvert>,
    .flags = 0};

} // namespace dlstreamer
