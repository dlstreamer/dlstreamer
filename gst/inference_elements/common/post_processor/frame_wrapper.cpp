/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "frame_wrapper.h"

#include "gva_base_inference.h"
#include <processor_types.h>

using namespace post_processing;

/* class FrameWrapper */

FrameWrapper::FrameWrapper(InferenceFrame &frame)
    : buffer(frame.buffer), model_instance_id(frame.gva_base_inference->model_instance_id), roi(&frame.roi),
      image_transform_info(frame.image_transform_info), width(frame.info->width), height(frame.info->height),
      roi_classifications(&frame.roi_classifications) {
}

// This constructor is only called for micro-elements, initialization of the rest of the fields is not required because
// they are not used there
FrameWrapper::FrameWrapper(GstBuffer *buf, const std::string &instance_id)
    : buffer(buf), model_instance_id(instance_id) {
}

/* class FramesWrapper */

FramesWrapper::FramesWrapper(const InferenceFrames &frames) : created_from_buf(false) {
    _frames.reserve(frames.size());
    for (auto &e : frames) {
        FrameWrapper fw(*e);
        _frames.push_back(fw);
    }
}

FramesWrapper::FramesWrapper(GstBuffer *buffer, const std::string &instance_id) : created_from_buf(true) {
    FrameWrapper fw(buffer, instance_id);
    _frames.push_back(fw);
}

bool FramesWrapper::empty() const {
    return _frames.empty();
}

size_t FramesWrapper::size() const {
    return _frames.size();
}

FrameWrapper &FramesWrapper::operator[](size_t i) {
    return _frames[i];
}

const FrameWrapper &FramesWrapper::operator[](size_t i) const {
    return _frames[i];
}

bool FramesWrapper::need_coordinate_restore() {
    return !created_from_buf;
}
