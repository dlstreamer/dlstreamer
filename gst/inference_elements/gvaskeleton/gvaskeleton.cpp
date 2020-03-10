#include "gvaskeleton.h"

#include "video_frame.h"

#include "gva_buffer_map.h"
#include "inference_backend/logger.h"
#include <gst/allocators/gstfdmemory.h>

#include "human_pose_estimator.hpp"
#include "render_human_pose.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace human_pose_estimation;

HumanPoseEstimator *hpe_initialization(char *model_path, char *device) {
    try {
        HumanPoseEstimator *hpe_obj = new HumanPoseEstimator(model_path, device);
        return hpe_obj;
    } catch (const std::exception &e) {
        GVA_ERROR(e.what());
    }
    return nullptr;
}
GvaSkeletonStatus hpe_release(HumanPoseEstimator *hpe_obj) {
    try {
        delete hpe_obj;
        hpe_obj = NULL;
        return GVA_SKELETON_OK;
    } catch (const std::exception &e) {
        GVA_ERROR(e.what());
        return GVA_SKELETON_ERROR;
    }
}

void convertPoses2Array(const std::vector<HumanPose> &poses, float *data, size_t kp_num) {
    size_t i = 0;
    for (const auto &pose : poses) {
        for (const auto &keypoint : pose.keypoints) {
            if (i >= kp_num) {
                std::runtime_error("Number of keypoints bigger then data's size");
            }
            data[i++] = keypoint.x;
            data[i++] = keypoint.y;
        }
    }
}
void copy_buffer_to_structure(GstStructure *structure, const void *buffer, int size) {
    ITT_TASK(__FUNCTION__);
    GVariant *v = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, buffer, size, 1);
    gsize n_elem;
    gst_structure_set(structure, "data_buffer", G_TYPE_VARIANT, v, "data", G_TYPE_POINTER,
                      g_variant_get_fixed_array(v, &n_elem, 1), NULL);
}

GvaSkeletonStatus attach_poses_to_buffer(const std::vector<HumanPose> &poses, GVA::VideoFrame &frame) {
    try {
        for (const auto &pose : poses) {
            auto human_pose_tensor = frame.add_tensor();
            human_pose_tensor.set_name("human_pose");
            human_pose_tensor.set_double("score", pose.score);
            human_pose_tensor.set_double("nose_x", pose.keypoints[0].x);
            human_pose_tensor.set_double("nose_y", pose.keypoints[0].y);
            human_pose_tensor.set_double("neck_x", pose.keypoints[1].x);
            human_pose_tensor.set_double("neck_y", pose.keypoints[1].y);
            human_pose_tensor.set_double("r_shoulder_x", pose.keypoints[2].x);
            human_pose_tensor.set_double("r_shoulder_y", pose.keypoints[2].y);
            human_pose_tensor.set_double("r_cubit_x", pose.keypoints[3].x);
            human_pose_tensor.set_double("r_cubit_y", pose.keypoints[3].y);
            human_pose_tensor.set_double("r_hand_x", pose.keypoints[4].x);
            human_pose_tensor.set_double("r_hand_y", pose.keypoints[4].y);
            human_pose_tensor.set_double("l_shoulder_x", pose.keypoints[5].x);
            human_pose_tensor.set_double("l_shoulder_y", pose.keypoints[5].y);
            human_pose_tensor.set_double("l_cubit_x", pose.keypoints[6].x);
            human_pose_tensor.set_double("l_cubit_y", pose.keypoints[6].y);
            human_pose_tensor.set_double("l_hand_x", pose.keypoints[7].x);
            human_pose_tensor.set_double("l_hand_y", pose.keypoints[7].y);
            human_pose_tensor.set_double("r_hip_x", pose.keypoints[8].x);
            human_pose_tensor.set_double("r_hip_y", pose.keypoints[8].y);
            human_pose_tensor.set_double("r_knee_x", pose.keypoints[9].x);
            human_pose_tensor.set_double("r_knee_y", pose.keypoints[9].y);
            human_pose_tensor.set_double("r_foot_x", pose.keypoints[10].x);
            human_pose_tensor.set_double("r_foot_y", pose.keypoints[10].y);
            human_pose_tensor.set_double("l_hip_x", pose.keypoints[11].x);
            human_pose_tensor.set_double("l_hip_y", pose.keypoints[11].y);
            human_pose_tensor.set_double("l_knee_x", pose.keypoints[12].x);
            human_pose_tensor.set_double("l_knee_y", pose.keypoints[12].y);
            human_pose_tensor.set_double("l_foot_x", pose.keypoints[13].x);
            human_pose_tensor.set_double("l_foot_y", pose.keypoints[13].y);
            human_pose_tensor.set_double("r_eye_x", pose.keypoints[14].x);
            human_pose_tensor.set_double("r_eye_y", pose.keypoints[14].y);
            human_pose_tensor.set_double("l_eye_x", pose.keypoints[15].x);
            human_pose_tensor.set_double("l_eye_y", pose.keypoints[15].y);
            human_pose_tensor.set_double("r_ear_x", pose.keypoints[16].x);
            human_pose_tensor.set_double("r_ear_y", pose.keypoints[16].y);
            human_pose_tensor.set_double("l_ear_x", pose.keypoints[17].x);
            human_pose_tensor.set_double("l_ear_y", pose.keypoints[17].y);
        }
        return GVA_SKELETON_OK;
    } catch (const std::exception &e) {
        GVA_ERROR(e.what());
    }
    return GVA_SKELETON_ERROR;
}

GvaSkeletonStatus attach_bbox_hands_to_buffer(const std::vector<HumanPose> &poses, GVA::VideoFrame &frame,
                                              size_t height, size_t width) {
    try {
        for (const auto &pose : poses) {
            if (pose.keypoints[4].x >= 0 and pose.keypoints[7].x >= 0 and pose.keypoints[3].x >= 0 and
                pose.keypoints[6].x >= 0 and pose.keypoints[4].y >= 0 and pose.keypoints[7].y >= 0 and
                pose.keypoints[3].y >= 0 and pose.keypoints[6].y >= 0) {
                auto right_hand = pose.keypoints[4];
                auto left_hand = pose.keypoints[7];
                auto right_cubit = pose.keypoints[3];
                auto left_cubit = pose.keypoints[6];

                float right_hand_bbox_size = cv::norm(right_hand - right_cubit);
                float left_hand_bbox_size = cv::norm(left_hand - left_cubit);

                float right_hand_bbox_x_min = right_hand.x - right_hand_bbox_size;
                float right_hand_bbox_y_min = right_hand.y - right_hand_bbox_size;

                float left_hand_bbox_x_min = left_hand.x - left_hand_bbox_size;
                float left_hand_bbox_y_min = left_hand.y - left_hand_bbox_size;

                left_hand_bbox_x_min = (left_hand_bbox_x_min >= 0) ? left_hand_bbox_x_min : 0;
                left_hand_bbox_y_min = (left_hand_bbox_y_min >= 0) ? left_hand_bbox_y_min : 0;

                float left_hand_bbox_x_max = (left_hand_bbox_x_min + left_hand_bbox_size * 2 <= width)
                                                 ? left_hand_bbox_x_min + left_hand_bbox_size * 2
                                                 : width;
                float left_hand_bbox_y_max = (left_hand_bbox_y_min + left_hand_bbox_size * 2 <= height)
                                                 ? left_hand_bbox_y_min + left_hand_bbox_size * 2
                                                 : height;

                auto left_hand_roi = frame.add_region(
                    static_cast<int>(left_hand_bbox_x_min), static_cast<int>(left_hand_bbox_y_min),
                    static_cast<int>(left_hand_bbox_x_max - left_hand_bbox_x_min),
                    static_cast<int>(left_hand_bbox_y_max - left_hand_bbox_y_min), 0, 0.99, nullptr, "left_hand");

                right_hand_bbox_x_min = (right_hand_bbox_x_min >= 0) ? right_hand_bbox_x_min : 0;
                right_hand_bbox_x_min = (right_hand_bbox_x_min <= width) ? right_hand_bbox_x_min : width;
                right_hand_bbox_y_min = (right_hand_bbox_y_min >= 0) ? right_hand_bbox_y_min : 0;
                right_hand_bbox_y_min = (right_hand_bbox_y_min <= height) ? right_hand_bbox_y_min : height;

                float right_hand_bbox_x_max = (right_hand_bbox_x_min + right_hand_bbox_size * 2 >= 0)
                                                  ? right_hand_bbox_x_min + right_hand_bbox_size * 2
                                                  : 0;
                right_hand_bbox_x_max = (right_hand_bbox_x_min + right_hand_bbox_size * 2 <= width)
                                            ? right_hand_bbox_x_min + right_hand_bbox_size * 2
                                            : width;
                float right_hand_bbox_y_max = (right_hand_bbox_y_min + right_hand_bbox_size * 2 >= 0)
                                                  ? right_hand_bbox_y_min + right_hand_bbox_size * 2
                                                  : 0;
                right_hand_bbox_y_max = (right_hand_bbox_y_min + right_hand_bbox_size * 2 <= height)
                                            ? right_hand_bbox_y_min + right_hand_bbox_size * 2
                                            : height;

                auto right_hand_roi = frame.add_region(
                    static_cast<int>(right_hand_bbox_x_min), static_cast<int>(right_hand_bbox_y_min),
                    static_cast<int>(right_hand_bbox_x_max - right_hand_bbox_x_min),
                    static_cast<int>(right_hand_bbox_y_max - right_hand_bbox_y_min), 1, 0.99, nullptr, "right_hand");
            }
        }
        return GVA_SKELETON_OK;
    } catch (const std::exception &e) {
        GVA_ERROR(e.what());
    }
    return GVA_SKELETON_ERROR;
}

GvaSkeletonStatus hpe_to_estimate(HumanPoseEstimator *hpe_obj, GstBuffer *buf, gboolean hands_detect,
                                  GstVideoInfo *info) {
    try {
        InferenceBackend::Image image{};
        BufferMapContext mapContext{};
        //  should be unmapped with gva_buffer_unmap() after usage
        gva_buffer_map(buf, image, mapContext, info, InferenceBackend::MemoryType::SYSTEM, GST_MAP_READ);

        const cv::Mat mat(image.height, image.width, CV_8UC3, image.planes[0], info->stride[0]);
        if (mat.empty())
            throw std::logic_error("cv::Mat (mapped buffer) is empty.");

        std::vector<HumanPose> poses = hpe_obj->estimate(mat);
        gva_buffer_unmap(buf, image, mapContext);

        if (!gst_buffer_is_writable(buf)) {
            GVA_DEBUG("Buffer is not writable. Trying to make it writable (may require copying)...")
            buf = gst_buffer_make_writable(buf);
        }

        GVA::VideoFrame frame(buf);
        if (attach_poses_to_buffer(poses, frame))
            throw std::runtime_error("Attaching human poses meta to buffer error.");
        if (hands_detect)
            if (attach_bbox_hands_to_buffer(poses, frame, image.height, image.width) == GVA_SKELETON_ERROR)
                throw std::runtime_error("Attaching hands bboxes meta to buffer error.");

        return GVA_SKELETON_OK;
    } catch (const std::exception &e) {
        GVA_ERROR(e.what());
    }
    return GVA_SKELETON_ERROR;
}