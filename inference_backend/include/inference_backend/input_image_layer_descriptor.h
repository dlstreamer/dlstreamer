/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "image.h"

#include <memory>
#include <vector>

namespace InferenceBackend {
class InputImageLayerDesc {
  public:
    using Ptr = std::shared_ptr<InputImageLayerDesc>;

    enum class Resize { NO, NO_ASPECT_RATIO, ASPECT_RATIO };
    enum class Crop { NO, CENTRAL, TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT };
    enum class ColorSpace { NO, RGB, BGR, YUV, GRAYSCALE };

    enum class Normalization { NO, RANGE, DISTRIBUTION };
    struct RangeNormalization {
      private:
        const bool defined = false;

      public:
        const double min = 0;
        const double max = 1;

        RangeNormalization() = default;
        RangeNormalization(double min, double max) : defined(true), min(min), max(max) {
        }

        bool isDefined() const {
            return defined;
        }
    };
    struct DistribNormalization {
      private:
        const bool defined = false;

      public:
        // standart normalization values for models, which was pretrained on ImageNet dataset
        const std::vector<double> mean = {0.485, 0.456, 0.406};
        const std::vector<double> std = {0.229, 0.224, 0.225};

        DistribNormalization() = default;
        DistribNormalization(const std::vector<double> mean, const std::vector<double> std)
            : defined(true), mean(mean), std(std) {
        }

        bool isDefined() const {
            return defined;
        }
    };

    InputImageLayerDesc() = default;
    InputImageLayerDesc(Resize resize, Crop crop, ColorSpace color_space)
        : resize(resize), crop(crop), color_space(color_space) {
        setDefaultToBlobSizeTransformationIsItNeed();
    }
    InputImageLayerDesc(Resize resize, Crop crop, ColorSpace color_space, double norm_range_min, double norm_range_max)
        : resize(resize), crop(crop), color_space(color_space), range_norm(norm_range_min, norm_range_max) {
        setDefaultToBlobSizeTransformationIsItNeed();
    }
    InputImageLayerDesc(Resize resize, Crop crop, ColorSpace color_space, double norm_range_min, double norm_range_max,
                        const std::vector<double> &mean, const std::vector<double> &std)
        : resize(resize), crop(crop), color_space(color_space), range_norm(norm_range_min, norm_range_max),
          distrib_norm(mean, std) {
        setDefaultToBlobSizeTransformationIsItNeed();
    }
    InputImageLayerDesc(Resize resize, Crop crop, ColorSpace color_space, const std::vector<double> &mean,
                        const std::vector<double> &std)
        : resize(resize), crop(crop), color_space(color_space), distrib_norm(mean, std) {
        setDefaultToBlobSizeTransformationIsItNeed();
    }
    InputImageLayerDesc(Resize resize, Crop crop, ColorSpace color_space, const RangeNormalization &norm)
        : resize(resize), crop(crop), color_space(color_space), range_norm(norm) {
        setDefaultToBlobSizeTransformationIsItNeed();
    }
    InputImageLayerDesc(Resize resize, Crop crop, ColorSpace color_space, const DistribNormalization &norm)
        : resize(resize), crop(crop), color_space(color_space), distrib_norm(norm) {
        setDefaultToBlobSizeTransformationIsItNeed();
    }
    InputImageLayerDesc(Resize resize, Crop crop, ColorSpace color_space, const RangeNormalization &range_norm,
                        const DistribNormalization &distrib_norm)
        : resize(resize), crop(crop), color_space(color_space), range_norm(range_norm), distrib_norm(distrib_norm) {
    }

    bool isTransformationToBlobSizeDefined() {
        if (resize != Resize::NO or crop != Crop::NO)
            return true;
        return false;
    }
    bool isDefined() {
        if (isTransformationToBlobSizeDefined() or color_space != ColorSpace::NO or range_norm.isDefined() or
            distrib_norm.isDefined())
            return true;
        return false;
    }
    bool doNeedResize() {
        return resize != Resize::NO;
    }
    Resize getResizeType() {
        return resize;
    }
    bool doNeedCrop() {
        if (crop == Crop::NO or resize == Resize::NO_ASPECT_RATIO)
            return false;
        return true;
    }
    Crop getCropType() {
        return crop;
    }
    bool doNeedColorSpaceConversion(ColorSpace src_color_space) {
        if (color_space == src_color_space or color_space == ColorSpace::NO)
            return false;
        return true;
    }
    bool doNeedColorSpaceConversion(int src_color_space) {
        if (color_space != ColorSpace::NO) {
            if (src_color_space == FOURCC_BGR and color_space == ColorSpace::BGR)
                return false;
            if (src_color_space == FOURCC_RGB and color_space == ColorSpace::RGB)
                return false;
            if (src_color_space == FOURCC_YUV and color_space == ColorSpace::YUV)
                return false;
        } else {
            return false;
        }
        return true;
    }
    ColorSpace getTargetColorSpace() {
        return color_space;
    }
    bool doNeedRangeNormalization() {
        return range_norm.isDefined();
    }
    const RangeNormalization &getRangeNormalization() {
        return range_norm;
    }
    bool doNeedDistribNormalization() {
        return distrib_norm.isDefined();
    }
    const DistribNormalization &getDistribNormalization() {
        return distrib_norm;
    }

  private:
    Resize resize;
    const Crop crop;
    const ColorSpace color_space;
    const RangeNormalization range_norm;
    const DistribNormalization distrib_norm;

    void setDefaultToBlobSizeTransformationIsItNeed() {
        if (isDefined() and not isTransformationToBlobSizeDefined()) {
            this->resize = Resize::NO_ASPECT_RATIO;
        }
    }
};

class ImageTransformationParams {
  protected:
    bool was_crop = false;
    bool was_aspect_ratio_resize = false;

  public:
    using Ptr = std::shared_ptr<ImageTransformationParams>;

    size_t cropped_frame_size_x = 0; //  eg 0 for *_Left crop; (src_size - dst_size) / 2 for CentralCrop
    size_t cropped_frame_size_y = 0; //  eg 0 for Top_* crop; (src_size - dst_size) / 2 for CentralCrop

    size_t resize_padding_size_x = 0; // padding is used by aspect_ratio resize
    size_t resize_padding_size_y = 0;
    double resize_scale_x = 1;
    double resize_scale_y = 1;

    bool WasTransformation() {
        return (was_aspect_ratio_resize || was_crop);
    }

    void CropHasDone(size_t _cropped_frame_size_x, size_t _cropped_frame_size_y) {
        was_crop = true;
        cropped_frame_size_x = _cropped_frame_size_x;
        cropped_frame_size_y = _cropped_frame_size_y;
    }
    bool WasCrop() {
        return was_crop;
    }

    void AspectRatioResizeHasDone(size_t _resize_padding_size_x, size_t _resize_padding_size_y, double _resize_scale_x,
                                  double _resize_scale_y) {
        was_aspect_ratio_resize = true;
        resize_padding_size_x = _resize_padding_size_x;
        resize_padding_size_y = _resize_padding_size_y;
        resize_scale_x = _resize_scale_x;
        resize_scale_y = _resize_scale_y;
    }
    bool WasAspectRatioResize() {
        return was_aspect_ratio_resize;
    }
};
} // namespace InferenceBackend
