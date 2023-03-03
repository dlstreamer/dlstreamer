/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __VAS_OT_H__
#define __VAS_OT_H__

#include "dlstreamer/utils.h"
#include "tracker.h"
#include <iostream>
#include <map>
#include <memory>
#include <opencv2/core.hpp>
#include <vector>

namespace vas {

/**
 * @namespace vas::ot
 * @brief %vas::ot namespace.
 *
 * The ot namespace has classes, functions, and definitions for object tracker.
 * It is a general tracker, and an object is represented as a rectangular box.
 * Thus, you can use any kind of detector if it generates a rectangular box as output.
 * Once an object is added to object tracker, the object is started to be tracked.
 */
namespace ot {

/**
 * @enum TrackingStatus
 *
 * Tracking status.
 */
enum class TrackingStatus {
    NEW,     /**< The object is newly added. */
    TRACKED, /**< The object is being tracked. */
    LOST     /**< The object gets lost now. The object can be tracked again automatically(long term tracking) or by
                specifying detected object manually(short term and zero term tracking). */
};

/**
 * @class DetectedObject
 * @brief Represents an input object.
 *
 * In order to track an object, detected object should be added one or more times to ObjectTracker.
 * When an object is required to be added to ObjectTracker, you can create an instance of this class and fill its
 * values.
 */
class DetectedObject {
  public:
    /**
     * Default constructor.
     */
    DetectedObject() : rect(), class_label() {
    }

    /**
     * Constructor with specific values.
     *
     * @param[in] input_rect Rectangle of input object.
     * @param[in] input_class_label Class label of input object.
     */
    DetectedObject(const cv::Rect &input_rect, int32_t input_class_label, const cv::Mat &feature)
        : rect(input_rect), class_label(input_class_label), feature(feature) {
    }

  public:
    /**
     * Object rectangle.
     */
    cv::Rect rect;

    /**
     * Input class label.
     * It is an arbitrary value that is specified by user.
     * You can utilize this value to categorize input objects.
     * Same value will be assigned to the class_label in Object class.
     */
    int32_t class_label;

    /**
     * Object feature vector.
     */
    cv::Mat feature;
};

/**
 * @class Object
 * @brief Represents tracking result of a target object.
 *
 * It contains tracking information of a target object.
 * ObjectTracker generates an instance of this class per tracked object when Track method is called.
 */
class Object {
  public:
    /**
     * Object rectangle.
     */
    cv::Rect rect;

    /**
     * Tracking ID.
     * Numbering sequence starts from 1.
     * The value 0 means the tracking ID of this object has not been assigned.
     */
    uint64_t tracking_id;

    /**
     * Class label.
     * This is specified by DetectedObject.
     */
    int32_t class_label;

    /**
     * Tracking status.
     */
    TrackingStatus status;

    /**
     * Index in the DetectedObject vector.
     * If the Object was not in detection input at this frame, then it will be -1.
     */
    int32_t association_idx;
};

/**
 * @class ObjectTracker
 * @brief Tracks objects from video frames.
 *
 * This class tracks objects from the input frames.
 * In order to create an instance of this class, you need to use ObjectTracker::Builder class.
 * @n
 * ObjectTracker can run in three different ways as TrackingType defines.
 * @n
 * In short term tracking, an object is added at the beginning, and the object is tracked with consecutive input frames.
 * It is recommended to update the tracked object's information for every 10-20 frames.
 * @n
 * Zero term tracking can be thought as association between a detected object and tracked object.
 * Detected objects should always be added when Track method is invoked.
 * For each frame, detected objects are mapped to tracked objects with this tracking type, which enables ID tracking of
 detected objects.
 * @n
 * Long term tracking is deprecated.
 * In long term tracking, an object is added at the beginning, and the object is tracked with consecutive input frames.
 * User doesn't need to update manually the object's information.
 * Long term tracking takes relatively long time to track objects.
 * @n
 * You can specify tracking type by setting attributes of Builder class when you create instances of this class.
 * It is not possible to run ObjectTracker with two or more different tracking types in one instance.
 * You can also limit the number of tracked objects by setting attributes of Builder class.
 * @n
 * Currently, ObjectTracker does not support HW offloading.
 * It is possible to run ObjectTracker only on CPU.
 * @n@n
 * Following sample code shows how to use short term tracking type.
 * Objects are added to ObjectTracker at the beginnning of tracking and in the middle of tracking periodically as well.
 * @code
    cv::VideoCapture video("/path/to/video/source");
    cv::Mat frame;
    cv::Mat first_frame;
    video >> first_frame;

    vas::ot::ObjectTracker::Builder ot_builder;
    auto ot = ot_builder.Build(vas::ot::TrackingType::SHORT_TERM);

    vas::pvd::PersonVehicleDetector::Builder pvd_builder;
    auto pvd = pvd_builder.Build("/path/to/directory/of/fd/model/files");

    std::vector<vas::pvd::PersonVehicle> person_vehicles;
    std::vector<vas::ot::DetectedObject> detected_objects;

    // Assume that there're objects in the first frame
    person_vehicles = pvd->Detect(first_frame);
    for (const auto& pv : person_vehicles)
        detected_objects.emplace_back(pv.rect, static_cast<int32_t>(pv.type));

    ot->Track(first_frame, detected_objects);

    // Assume that now pvd is running in another thread
    StartThread(pvd);

    while (video.read(frame))
    {
        detected_objects.clear();

        // Assume that frames are forwarded to the thread on which pvd is running
        EnqueueFrame(frame);

        // Assumes that pvd is adding its result into a queue in another thread.
        // Assumes also that latency from the last pvd frame to current frame is ignorable.
        person_vehicles = DequeuePersonVehicles();
        if (!person_vehicles.empty())
        {
            detected_objects.clear();
            for (const auto& pv : person_vehicles)
                detected_objects.emplace_back(pv.rect, static_cast<int32_t>(pv.type));
        }

        auto objects = ot->Track(frame, detected_objects);
        for (const auto& object : objects)
        {
            // Handle tracked object
        }
    }
 * @endcode
 * @n
 * Following sample code shows how to use zero term tracking type.
 * In this sample, pvd runs for each input frame.
 * After pvd generates results, ot runs with the results and object IDs are preserved.
 * @code
    cv::VideoCapture video("/path/to/video/source");
    cv::Mat frame;

    vas::ot::ObjectTracker::Builder ot_builder;
    auto ot = ot_builder.Build(vas::ot::TrackingType::ZERO_TERM);

    vas::pvd::PersonVehicleDetector::Builder pvd_builder;
    auto pvd = pvd_builder.Build("/path/to/directory/of/fd/model/files");

    std::vector<vas::ot::DetectedObject> detected_objects;

    ot->SetFrameDeltaTime(0.033f);
    while (video.read(frame))
    {
        detected_objects.clear();

        auto person_vehicles = pvd->Detect(first_frame);
        for (const auto& pv : person_vehicles)
            detected_objects.emplace_back(pv.rect, static_cast<int32_t>(pv.type));

        auto objects = ot->Track(frame, detected_objects);
        for (const auto& object : objects)
        {
            // Handle tracked object
        }
    }
 * @endcode
 */
class ObjectTracker {
  public:
    using InitParameters = vas::ot::Tracker::InitParameters;

  public:
    explicit ObjectTracker(const InitParameters &param);

    ObjectTracker() = delete;
    ~ObjectTracker();
    ObjectTracker(const ObjectTracker &) = delete;
    ObjectTracker(ObjectTracker &&) = delete;
    ObjectTracker &operator=(const ObjectTracker &) = delete;
    ObjectTracker &operator=(ObjectTracker &&) = delete;

  public:
    void SetDeltaTime(float delta_t);
    std::vector<Object> Track(cv::Size frame_size, const std::vector<DetectedObject> &objects);

  private:
    std::unique_ptr<vas::ot::Tracker> tracker_;
    std::vector<std::shared_ptr<Tracklet>> produced_tracklets_;

    float delta_t_;
    bool tracking_per_class_;
};

}; // namespace ot
}; // namespace vas

#endif // __VAS_OT_H__
