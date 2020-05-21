/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "trackskeleton.h"
#include <math.h>

using namespace skeletontracker;
Tracker::Tracker(int _frame_width, int _frame_height, std::vector<std::map<std::string, float>> _poses, int _object_id,
                 float _threshold)
    : poses(_poses), object_id(_object_id), threshold(_threshold), frame_width(_frame_width),
      frame_height(_frame_height) {
}

ITracker *Tracker::Create(const GstVideoInfo *video_info) {
    return new Tracker(video_info->width, video_info->height);
}

void Tracker::track(GstBuffer *buffer) {
    GVA::VideoFrame frame(buffer);
    if (poses.empty()) {
        for (auto &tensor : frame.tensors()) {
            if (tensor.is_human_pose()) {
                tensor.set_int("object_id", ++object_id);
            }
        }
        copyTensorsToPoses(frame.tensors(), poses);
    } else {
        std::map<float, std::map<std::string, float>> matches;
        for (auto &tensor : frame.tensors()) {
            if (tensor.is_human_pose())
                for (auto &pose : poses) {
                    // std::cout << "pose " << pose["r_shoulder_x"] << " tensor " << tensor.get_double("r_shoulder_x")
                    //           << std::endl;
                    matches.insert({Distance(tensor, pose), pose});
                }
            if (matches.begin()->first < threshold) {
                tensor.set_int("object_id", static_cast<int>(matches.begin()->second["object_id"]));
            } else {
                tensor.set_int("object_id", ++object_id);
            }
            matches.clear();
        }
        poses.clear();
        copyTensorsToPoses(frame.tensors(), poses);
    }
}

float Tracker::Distance(const GVA::Tensor &tensor, const std::map<std::string, float> &pose) {

    float nose = sqrt(pow((tensor.get_double("nose_x") - pose.at("nose_x")) / frame_width, 2) +
                      pow((tensor.get_double("nose_y") - pose.at("nose_y")) / frame_height, 2));
    float neck = sqrt(pow((tensor.get_double("neck_x") - pose.at("neck_x")) / frame_width, 2) +
                      pow((tensor.get_double("neck_y") - pose.at("neck_y")) / frame_height, 2));
    float r_shoulder = sqrt(pow((tensor.get_double("r_shoulder_x") - pose.at("r_shoulder_x")) / frame_width, 2) +
                            pow((tensor.get_double("r_shoulder_y") - pose.at("r_shoulder_y")) / frame_height, 2));
    float r_cubit = sqrt(pow((tensor.get_double("r_cubit_x") - pose.at("r_cubit_x")) / frame_width, 2) +
                         pow((tensor.get_double("r_cubit_y") - pose.at("r_cubit_y")) / frame_height, 2));
    float r_hand = sqrt(pow((tensor.get_double("r_hand_x") - pose.at("r_hand_x")) / frame_width, 2) +
                        pow((tensor.get_double("r_hand_y") - pose.at("r_hand_y")) / frame_height, 2));
    float l_shoulder = sqrt(pow((tensor.get_double("l_shoulder_x") - pose.at("l_shoulder_x")) / frame_width, 2) +
                            pow((tensor.get_double("l_shoulder_y") - pose.at("l_shoulder_y")) / frame_height, 2));
    float l_cubit = sqrt(pow((tensor.get_double("l_cubit_x") - pose.at("l_cubit_x")) / frame_width, 2) +
                         pow((tensor.get_double("l_cubit_y") - pose.at("l_cubit_y")) / frame_height, 2));
    float l_hand = sqrt(pow((tensor.get_double("l_hand_x") - pose.at("l_hand_x")) / frame_width, 2) +
                        pow((tensor.get_double("l_hand_y") - pose.at("l_hand_y")) / frame_height, 2));
    float r_hip = sqrt(pow((tensor.get_double("r_hip_x") - pose.at("r_hip_x")) / frame_width, 2) +
                       pow((tensor.get_double("r_hip_y") - pose.at("r_hip_y")) / frame_height, 2));
    float r_knee = sqrt(pow((tensor.get_double("r_knee_x") - pose.at("r_knee_x")) / frame_width, 2) +
                        pow((tensor.get_double("r_knee_y") - pose.at("r_knee_y")) / frame_height, 2));
    float r_foot = sqrt(pow((tensor.get_double("r_foot_x") - pose.at("r_foot_x")) / frame_width, 2) +
                        pow((tensor.get_double("r_foot_y") - pose.at("r_foot_y")) / frame_height, 2));
    float l_hip = sqrt(pow((tensor.get_double("l_hip_x") - pose.at("l_hip_x")) / frame_width, 2) +
                       pow((tensor.get_double("l_hip_y") - pose.at("l_hip_y")) / frame_height, 2));
    float l_knee = sqrt(pow((tensor.get_double("l_knee_x") - pose.at("l_knee_x")) / frame_width, 2) +
                        pow((tensor.get_double("l_knee_y") - pose.at("l_knee_y")) / frame_height, 2));
    float l_foot = sqrt(pow((tensor.get_double("l_foot_x") - pose.at("l_foot_x")) / frame_width, 2) +
                        pow((tensor.get_double("l_foot_y") - pose.at("l_foot_y")) / frame_height, 2));
    float r_eye = sqrt(pow((tensor.get_double("r_eye_x") - pose.at("r_eye_x")) / frame_width, 2) +
                       pow((tensor.get_double("r_eye_y") - pose.at("r_eye_y")) / frame_height, 2));
    float l_eye = sqrt(pow((tensor.get_double("l_eye_x") - pose.at("l_eye_x")) / frame_width, 2) +
                       pow((tensor.get_double("l_eye_y") - pose.at("l_eye_y")) / frame_height, 2));
    float r_ear = sqrt(pow((tensor.get_double("r_ear_x") - pose.at("r_ear_x")) / frame_width, 2) +
                       pow((tensor.get_double("r_ear_y") - pose.at("r_ear_y")) / frame_height, 2));
    float l_ear = sqrt(pow((tensor.get_double("l_ear_x") - pose.at("l_ear_x")) / frame_width, 2) +
                       pow((tensor.get_double("l_ear_y") - pose.at("l_ear_y")) / frame_height, 2));

    float distance = (nose + neck + r_shoulder + r_cubit + r_hand + l_shoulder + l_cubit + l_hand + r_hip + r_knee +
                      r_foot + l_hip + l_knee + l_foot + r_eye + l_eye + r_ear + l_ear) /
                     18;
    return distance;
}
void Tracker::copyTensorsToPoses(const std::vector<GVA::Tensor> &tensors,
                                 std::vector<std::map<std::string, float>> &poses) {
    size_t i = 0;
    for (const auto &tensor : tensors) {
        if (tensor.is_human_pose()) {
            poses.reserve(tensors.size());
            std::map<std::string, float> pose_map;
            pose_map.insert({"nose_x", tensor.get_double("nose_x")});
            pose_map.insert({"nose_y", tensor.get_double("nose_y")});
            pose_map.insert({"neck_x", tensor.get_double("neck_x")});
            pose_map.insert({"neck_y", tensor.get_double("neck_y")});
            pose_map.insert({"r_shoulder_x", tensor.get_double("r_shoulder_x")});
            pose_map.insert({"r_shoulder_y", tensor.get_double("r_shoulder_y")});
            pose_map.insert({"r_cubit_x", tensor.get_double("r_cubit_x")});
            pose_map.insert({"r_cubit_y", tensor.get_double("r_cubit_y")});
            pose_map.insert({"r_hand_x", tensor.get_double("r_hand_x")});
            pose_map.insert({"r_hand_y", tensor.get_double("r_hand_y")});
            pose_map.insert({"l_shoulder_x", tensor.get_double("l_shoulder_x")});
            pose_map.insert({"l_shoulder_y", tensor.get_double("l_shoulder_y")});
            pose_map.insert({"l_cubit_x", tensor.get_double("l_cubit_x")});
            pose_map.insert({"l_cubit_y", tensor.get_double("l_cubit_y")});
            pose_map.insert({"l_hand_x", tensor.get_double("l_hand_x")});
            pose_map.insert({"l_hand_y", tensor.get_double("l_hand_y")});
            pose_map.insert({"r_hip_x", tensor.get_double("r_hip_x")});
            pose_map.insert({"r_hip_y", tensor.get_double("r_hip_y")});
            pose_map.insert({"r_knee_x", tensor.get_double("r_knee_x")});
            pose_map.insert({"r_knee_y", tensor.get_double("r_knee_y")});
            pose_map.insert({"r_foot_x", tensor.get_double("r_foot_x")});
            pose_map.insert({"r_foot_y", tensor.get_double("r_foot_y")});
            pose_map.insert({"l_hip_x", tensor.get_double("l_hip_x")});
            pose_map.insert({"l_hip_y", tensor.get_double("l_hip_y")});
            pose_map.insert({"l_knee_x", tensor.get_double("l_knee_x")});
            pose_map.insert({"l_knee_y", tensor.get_double("l_knee_y")});
            pose_map.insert({"l_foot_x", tensor.get_double("l_foot_x")});
            pose_map.insert({"l_foot_y", tensor.get_double("l_foot_y")});
            pose_map.insert({"r_eye_x", tensor.get_double("r_eye_x")});
            pose_map.insert({"r_eye_y", tensor.get_double("r_eye_y")});
            pose_map.insert({"l_eye_x", tensor.get_double("l_eye_x")});
            pose_map.insert({"l_eye_y", tensor.get_double("l_eye_y")});
            pose_map.insert({"r_ear_x", tensor.get_double("r_ear_x")});
            pose_map.insert({"r_ear_y", tensor.get_double("r_ear_y")});
            pose_map.insert({"l_ear_x", tensor.get_double("l_ear_x")});
            pose_map.insert({"l_ear_y", tensor.get_double("l_ear_y")});
            pose_map.insert({"object_id", tensor.get_int("object_id")});
            poses.push_back(pose_map);
        }
        ++i;
    }
}