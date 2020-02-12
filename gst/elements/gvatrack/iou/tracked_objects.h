/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <opencv2/core/core.hpp>
#include <vector>

namespace iou {

struct TrackedObject {
    cv::Rect rect;
    float confidence;

    int label; // either id of a label, or UNKNOWN_LABEL_IDX
    int object_index;
    int object_id;
    static const int UNKNOWN_LABEL_IDX; // the value (-1) for unknown label

    int frame_idx; ///< Frame index where object was detected (-1 if N/A).

    TrackedObject(const cv::Rect &rect = cv::Rect(), float conf = -1.0f, int label = -1, int object_index = -1,
                  int object_id = -1)
        : rect(rect), confidence(conf), label(label), object_index(object_index), object_id(object_id), frame_idx(-1) {
    }
};

using TrackedObjects = std::vector<TrackedObject>;

///
/// \brief The Track struct describes tracks.
///
struct Track {
    ///
    /// \brief Track constructor.
    /// \param objs Detected objects sequence.
    ///
    explicit Track(const TrackedObjects &objs) : objects(objs), lost(0), length(1) {
        CV_Assert(!objs.empty());
        first_object = objs[0];
    }

    ///
    /// \brief empty returns if track does not contain objects.
    /// \return true if track does not contain objects.
    ///
    bool empty() const {
        return objects.empty();
    }

    ///
    /// \brief size returns number of detected objects in a track.
    /// \return number of detected objects in a track.
    ///
    size_t size() const {
        return objects.size();
    }

    ///
    /// \brief operator [] return const reference to detected object with
    ///        specified index.
    /// \param i Index of object.
    /// \return const reference to detected object with specified index.
    ///
    const TrackedObject &operator[](size_t i) const {
        return objects[i];
    }

    ///
    /// \brief operator [] return non-const reference to detected object with
    ///        specified index.
    /// \param i Index of object.
    /// \return non-const reference to detected object with specified index.
    ///
    TrackedObject &operator[](size_t i) {
        return objects[i];
    }

    ///
    /// \brief back returns const reference to last object in track.
    /// \return const reference to last object in track.
    ///
    const TrackedObject &back() const {
        CV_Assert(!empty());
        return objects.back();
    }

    ///
    /// \brief back returns non-const reference to last object in track.
    /// \return non-const reference to last object in track.
    ///
    TrackedObject &back() {
        CV_Assert(!empty());
        return objects.back();
    }

    TrackedObjects objects; ///< Detected objects;
    size_t lost;            ///< How many frames ago track has been lost.

    TrackedObject first_object; ///< First object in track.
    size_t length;              ///< Length of a track including number of objects that were
                                /// removed from track in order to avoid memory usage growth.
};

} // namespace iou
