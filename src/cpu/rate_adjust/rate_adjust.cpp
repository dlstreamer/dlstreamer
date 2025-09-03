/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/cpu/elements/rate_adjust.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/image_metadata.h"

namespace dlstreamer {

namespace param {
static constexpr auto ratio = "ratio";
}; // namespace param

auto default_ratio = std::pair<int, int>(1, 1);

static ParamDescVector params_desc = {
    {param::ratio,
     "Frame rate ratio - output frame rate is input rate multiplied by specified ratio."
     " Current limitation: ratio <= 1",
     default_ratio, std::pair<int, int>(0, 1), std::pair<int, int>(1, 1)}};

class RateAdjust : public BaseTransformInplace {
  public:
    RateAdjust(DictionaryCPtr params, const ContextPtr &app_context) : BaseTransformInplace(app_context) {
        auto ratio = params->get(param::ratio, default_ratio);
        _numerator = ratio.first;
        _denominator = ratio.second;
        _bypass = _numerator == 1 && _denominator == 1;
    }

    bool process(FramePtr frame) override {
        if (_bypass)
            return true;

        // in case of object classification after object tracking, adjust rate separately per each object_id
        auto source_id_meta = find_metadata<SourceIdentifierMetadata>(*frame);
        int object_id = source_id_meta ? source_id_meta->object_id() : 0;

        int64_t &frames_total = _frames_total[object_id];
        int64_t &frames_accepted = _frames_accepted[object_id];

        frames_total++;
        // Formula: frames_accepted/frames_total < _numerator/_denominator
        bool accepted = frames_accepted * _denominator < frames_total * _numerator;
        if (accepted)
            frames_accepted++;

#if 0 // formula works ok even without resetting values
        if (frames_total >= _denominator) {
            frames_total = 0;
            frames_accepted = 0;
        }
#endif
        return accepted;
    }

  private:
    int64_t _numerator = 1;
    int64_t _denominator = 1;
    bool _bypass = false;
    std::map<int, int64_t> _frames_total;
    std::map<int, int64_t> _frames_accepted;
};

extern "C" {
ElementDesc rate_adjust = {
    .name = "rate_adjust",
    .description = "Adjust frame rate. Output frame rate is input rate multiplied by (numerator/denominator)",
    .author = "Intel Corporation",
    .params = &params_desc,
    .input_info = MAKE_FRAME_INFO_VECTOR({}),
    .output_info = MAKE_FRAME_INFO_VECTOR({}),
    .create = create_element<RateAdjust>,
    .flags = 0};
}

} // namespace dlstreamer
