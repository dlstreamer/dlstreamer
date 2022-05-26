/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "rate_adjust.h"
#include "dlstreamer/metadata.h"
#include "dlstreamer/utils.h"

namespace dlstreamer {

namespace param {
static constexpr auto numerator = "numerator";
static constexpr auto denominator = "denominator";
}; // namespace param

static ParamDescVector params_desc = {
    {param::numerator,
     "numerator value - output frame rate is input rate multiplied by (numerator/denominator)."
     " Current limitation: numerator <= denominator",
     1, 1, INT32_MAX},
    {param::denominator, "denominator value - output frame rate is input rate multiplied by (numerator/denominator)", 1,
     1, INT32_MAX}};

class RateAdjust : public TransformInplace {
  public:
    RateAdjust(ITransformController &transform_ctrl, DictionaryCPtr params)
        : TransformInplace(transform_ctrl, std::move(params)) {
        _numerator = _params->get<int>(param::numerator, 1);
        _denominator = _params->get<int>(param::denominator, 1);
        _bypass = _numerator == 1 && _denominator == 1;
    }

    void set_info(const BufferInfo &, const BufferInfo &) override {
    }

    bool process(BufferPtr buffer) override {
        if (_bypass)
            return true;

        // in case of object classification after object tracking, adjust rate separately per each object_id
        auto source_id_meta = find_metadata<SourceIdentifierMetadata>(*buffer);
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

TransformDesc RateAdjustDesc = {
    .name = "rate_adjust",
    .description = "Adjust frame rate. Output frame rate is input rate multiplied by (numerator/denominator)",
    .author = "Intel Corporation",
    .params = &params_desc,
    .input_info = {MediaType::VIDEO, MediaType::TENSORS},
    .output_info = {MediaType::VIDEO, MediaType::TENSORS},
    .create = TransformBase::create<RateAdjust>,
    .flags = 0};

} // namespace dlstreamer
