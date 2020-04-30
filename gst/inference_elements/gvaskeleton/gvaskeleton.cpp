#include "gvaskeleton.h"

#include "gva_buffer_map.h"
#include "gva_utils.h"
#include "human_pose_estimator.hpp"
#include "inference_backend/logger.h"
#include "render_human_pose.hpp"
#include "video_frame.h"
#include <fstream>
#include <gst/allocators/gstfdmemory.h>

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

GvaSkeletonStatus attach_bbox_body_to_buffer(const std::vector<HumanPose> &poses, GVA::VideoFrame &frame, size_t height,
                                             size_t width) {
    try {
        for (const auto &pose : poses) {
            float max_keypoint_x = -1.0f;
            float max_keypoint_y = -1.0f;
            float min_keypoint_x = width;
            float min_keypoint_y = height;
            const cv::Point2f absentKeypoint(-1.0f, -1.0f);
            for (const auto &keypoint : pose.keypoints) {
                if (keypoint == absentKeypoint) {
                    continue;
                }
                if (keypoint.x > max_keypoint_x)
                    max_keypoint_x = keypoint.x;
                if (keypoint.y > max_keypoint_y)
                    max_keypoint_y = keypoint.y;

                if (keypoint.x < min_keypoint_x)
                    min_keypoint_x = keypoint.x;
                if (keypoint.y < min_keypoint_y)
                    min_keypoint_y = keypoint.y;
            }
            auto right_hand_roi =
                frame.add_region(static_cast<int>(min_keypoint_x), static_cast<int>(min_keypoint_y),
                                 static_cast<int>(max_keypoint_x - min_keypoint_x),
                                 static_cast<int>(max_keypoint_y - min_keypoint_y), 1, 0.99, nullptr, "body");
        }
        return GVA_SKELETON_OK;
    } catch (const std::exception &e) {
        GVA_ERROR(e.what());
    }
    return GVA_SKELETON_ERROR;
}

GvaSkeletonStatus attach_bbox_id_to_skeleton(const std::vector<HumanPose> &poses, GVA::VideoFrame &frame) {
    try {
        for (const auto &roi : frame.regions()) {
            // g_print("label_id %d\n", roi.label_id());
            GstVideoRegionOfInterestMeta *roi_meta = roi.meta();
            for (auto &pose : frame.tensors()) {
                uint points_counter = 0;
                if (pose.is_human_pose()) {
                    if (pose.get_double("nose_x") <= roi_meta->w && pose.get_double("nose_y") <= roi_meta->h)
                        points_counter++;
                    if (pose.get_double("r_shoulder_x") <= roi_meta->w &&
                        pose.get_double("r_shoulder_y") <= roi_meta->h)
                        points_counter++;
                    if (pose.get_double("r_cubit_x") <= roi_meta->w && pose.get_double("r_cubit_y") <= roi_meta->h)
                        points_counter++;
                    if (pose.get_double("r_hand_x") <= roi_meta->w && pose.get_double("r_hand_y") <= roi_meta->h)
                        points_counter++;
                    if (pose.get_double("r_hip_x") <= roi_meta->w && pose.get_double("r_hip_y") <= roi_meta->h)
                        points_counter++;
                    if (pose.get_double("r_knee_x") <= roi_meta->w && pose.get_double("r_knee_y") <= roi_meta->h)
                        points_counter++;
                    if (pose.get_double("r_foot_x") <= roi_meta->w && pose.get_double("r_foot_y") <= roi_meta->h)
                        points_counter++;
                    if (pose.get_double("r_eye_x") <= roi_meta->w && pose.get_double("r_eye_y") <= roi_meta->h)
                        points_counter++;
                    if (pose.get_double("r_ear_x") <= roi_meta->w && pose.get_double("r_ear_y") <= roi_meta->h)
                        points_counter++;
                    if (pose.get_double("l_shoulder_x") <= roi_meta->w &&
                        pose.get_double("l_shoulder_y") <= roi_meta->h)
                        points_counter++;
                    if (pose.get_double("l_cubit_x") <= roi_meta->w && pose.get_double("l_cubit_y") <= roi_meta->h)
                        points_counter++;
                    if (pose.get_double("l_hand_x") <= roi_meta->w && pose.get_double("l_hand_y") <= roi_meta->h)
                        points_counter++;
                    if (pose.get_double("l_hip_x") <= roi_meta->w && pose.get_double("l_hip_y") <= roi_meta->h)
                        points_counter++;
                    if (pose.get_double("l_knee_x") <= roi_meta->w && pose.get_double("l_knee_y") <= roi_meta->h)
                        points_counter++;
                    if (pose.get_double("l_foot_x") <= roi_meta->w && pose.get_double("l_foot_y") <= roi_meta->h)
                        points_counter++;
                    if (pose.get_double("l_eye_x") <= roi_meta->w && pose.get_double("l_eye_y") <= roi_meta->h)
                        points_counter++;
                    if (pose.get_double("l_ear_x") <= roi_meta->w && pose.get_double("l_ear_y") <= roi_meta->h)
                        points_counter++;
                }
                if (points_counter > 5 && points_counter <= 18) {
                    // g_print("points_counter %d\n", points_counter);
                    int id = 0;
                    if (get_object_id(roi_meta, &id))
                        pose.set_int("pose_id", id);
                }
            }
        }
        return GVA_SKELETON_OK;
    } catch (const std::exception &e) {
        GVA_ERROR(e.what());
    }
    return GVA_SKELETON_ERROR;
}

GvaSkeletonStatus attach_bbox_head_to_buffer(const std::vector<HumanPose> &poses, GVA::VideoFrame &frame, size_t height,
                                             size_t width) {
    try {
        for (const auto &pose : poses) {
            float left_top_angle_x = -1.0f;
            float left_top_angle_y = -1.0f;
            float bb_height = 0;
            float bb_width = 0;
            float dist_betw_mid_and_neck = 0.0f;
            const cv::Point2f absentKeypoint(-1.0f, -1.0f);
            if (pose.keypoints[1] != absentKeypoint && pose.keypoints[16] != absentKeypoint &&
                pose.keypoints[17] != absentKeypoint) {
                if (pose.keypoints[16].x < pose.keypoints[17].x) {
                    if (pose.keypoints[16].y > 0) {
                        dist_betw_mid_and_neck = pose.keypoints[1].y - pose.keypoints[16].y;
                        left_top_angle_y = pose.keypoints[16].y - dist_betw_mid_and_neck;
                    } else {
                        dist_betw_mid_and_neck = pose.keypoints[1].y - pose.keypoints[14].y;
                        left_top_angle_y = pose.keypoints[14].y - dist_betw_mid_and_neck;
                    }
                    left_top_angle_x = pose.keypoints[16].x;
                    bb_width = pose.keypoints[17].x - pose.keypoints[16].x;
                } else {
                    if (pose.keypoints[16].y > 0) {
                        dist_betw_mid_and_neck = pose.keypoints[1].y - pose.keypoints[17].y;
                        left_top_angle_y = pose.keypoints[17].y - dist_betw_mid_and_neck;
                    } else {
                        dist_betw_mid_and_neck = pose.keypoints[1].y - pose.keypoints[15].y;
                        left_top_angle_y = pose.keypoints[15].y - dist_betw_mid_and_neck;
                    }
                    left_top_angle_x = pose.keypoints[17].x;
                    bb_width = pose.keypoints[16].x - pose.keypoints[17].x;
                }

                bb_height = pose.keypoints[1].y - left_top_angle_y;
            }
            if (left_top_angle_x < 0)
                left_top_angle_x = 0;
            if (left_top_angle_y < 0)
                left_top_angle_y = 0;
            if (left_top_angle_y + bb_height > height)
                bb_height = height - left_top_angle_y;
            if (left_top_angle_x + bb_width > width)
                bb_width = width - left_top_angle_x;

            auto head_roi =
                frame.add_region(static_cast<int>(left_top_angle_x), static_cast<int>(left_top_angle_y),
                                 static_cast<int>(bb_width), static_cast<int>(bb_height), 1, 0.99, nullptr, "head");
            /* GST_DEBUG=3 gst-launch-1.0 filesrc
             * location=/home/pbochenk/projects/diplom/video-samples/t_video5262908514833007730.mp4 ! qtdemux !
             * avdec_h264 ! videoconvert ! video/x-raw,format=BGR ! gvaskeleton
             * model_path=/home/pbochenk/projects/models/intel/Transportation/human_pose_estimation/mobilenet-v1/dldt/human-pose-estimation-0001.xml
             * body-detect=false ! queue ! gvatrack tracking-type=short-term !  gvawatermark ! videoconvert ! fakesink
             * sync=false*/
            /*gst-launch-1.0 filesrc
             * location=/home/pbochenk/projects/diplom/video-samples/t_video5262908514833007730.mp4 ! qtdemux !
             * avdec_h264 ! videoconvert ! video/x-raw,format=BGR ! gvadetect
             * model=/home/pbochenk/projects/models/test_benchmark_models/openVINO/2019.1.1/Retail/object_detection/pedestrian/rmnet_ssd/0013/dldt/person-detection-retail-0013.xml
             * ! gvatrack tracking-type=short-term ! gvaskeleton
             * model_path=/home/pbochenk/projects/models/intel/Transportation/human_pose_estimation/mobilenet-v1/dldt/human-pose-estimation-0001.xml
             * body-detect=false ! gvawatermark ! videoconvert ! fakesink sync=false >>
             * /home/pbochenk/projects/diplom/keypoints_with_it.txt*/
        }
        return GVA_SKELETON_OK;
    } catch (const std::exception &e) {
        GVA_ERROR(e.what());
    }
    return GVA_SKELETON_ERROR;
}

void print_points_with_id(GstBuffer *buffer) {
    GVA::VideoFrame video_frame(buffer);
    for (GVA::Tensor &human_pose_tensor : video_frame.tensors()) {
        g_print("pose id %d %s", human_pose_tensor.get_int("pose_id"), " : ");

        g_print(" { nose %f %s %f %s", human_pose_tensor.get_double("nose_x"), " ",
                human_pose_tensor.get_double("nose_y"), "; ");
        g_print("  neck %f %s %f %s", human_pose_tensor.get_double("neck_x"), " ",
                human_pose_tensor.get_double("neck_y"), "; ");
        g_print("  r_shoulder %f %s %f %s", human_pose_tensor.get_double("r_shoulder_x"), " ",
                human_pose_tensor.get_double("r_shoulder_y"), "; ");
        g_print("  r_cubit %f %s %f %s", human_pose_tensor.get_double("r_cubit_x"), " ",
                human_pose_tensor.get_double("r_cubit_y"), "; ");
        g_print("  r_hand %f %s %f %s", human_pose_tensor.get_double("r_hand_x"), " ",
                human_pose_tensor.get_double("r_hand_y"), "; ");
        g_print("  l_shoulder %f %s %f %s", human_pose_tensor.get_double("l_shoulder_x"), " ",
                human_pose_tensor.get_double("l_shoulder_y"), "; ");
        g_print("  l_cubit %f %s %f %s", human_pose_tensor.get_double("l_cubit_x"), " ",
                human_pose_tensor.get_double("l_cubit_y"), "; ");
        g_print("  l_hand %f %s %f %s", human_pose_tensor.get_double("l_hand_x"), " ",
                human_pose_tensor.get_double("l_hand_y"), "; ");
        g_print("  r_hip %f %s %f %s", human_pose_tensor.get_double("r_hip_x"), " ",
                human_pose_tensor.get_double("r_hip_y"), "; ");
        g_print("  r_knee %f %s %f %s", human_pose_tensor.get_double("r_knee_x"), " ",
                human_pose_tensor.get_double("r_knee_y"), "; ");
        g_print("  r_foot %f %s %f %s", human_pose_tensor.get_double("r_foot_x"), " ",
                human_pose_tensor.get_double("r_foot_y"), "; ");
        g_print("  l_hip %f %s %f %s", human_pose_tensor.get_double("l_hip_x"), " ",
                human_pose_tensor.get_double("l_hip_y"), "; ");
        g_print("  l_knee %f %s %f %s", human_pose_tensor.get_double("l_knee_x"), " ",
                human_pose_tensor.get_double("l_knee_y"), "; ");
        g_print("  l_foot %f %s %f %s", human_pose_tensor.get_double("l_foot_x"), " ",
                human_pose_tensor.get_double("l_foot_y"), "; ");
        g_print("  r_eye %f %s %f %s", human_pose_tensor.get_double("r_eye_x"), " ",
                human_pose_tensor.get_double("r_eye_y"), "; ");
        g_print("  l_eye %f %s %f %s", human_pose_tensor.get_double("l_eye_x"), " ",
                human_pose_tensor.get_double("l_eye_y"), "; ");
        g_print("  r_ear %f %s %f %s", human_pose_tensor.get_double("r_ear_x"), " ",
                human_pose_tensor.get_double("r_ear_y"), "; ");
        g_print("  l_ear %f %s %f\n", human_pose_tensor.get_double("l_ear_x"), " ",
                human_pose_tensor.get_double("l_ear_y"));
    }
}

GvaSkeletonStatus hpe_to_estimate(HumanPoseEstimator *hpe_obj, GstBuffer *buf, gboolean hands_detect,
                                  gboolean body_detect, GstVideoInfo *info) {
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
        if (attach_poses_to_buffer(poses, frame) == GVA_SKELETON_ERROR)
            throw std::runtime_error("Attaching pose meta to buffer error.");
        if (hands_detect)
            if (attach_bbox_hands_to_buffer(poses, frame, image.height, image.width) == GVA_SKELETON_ERROR)
                throw std::runtime_error("Attaching hands bboxes meta to buffer error.");
        // if (attach_bbox_id_to_skeleton(poses, frame) == GVA_SKELETON_ERROR)
        //     throw std::runtime_error("Attaching id meta to buffer error.");
        if (body_detect)
            if (attach_bbox_body_to_buffer(poses, frame, image.height, image.width) == GVA_SKELETON_ERROR)
                throw std::runtime_error("Attaching body bboxes meta to buffer error.");
        // print_points_with_id(buf);
        return GVA_SKELETON_OK;
    } catch (const std::exception &e) {
        GVA_ERROR(e.what());
    }
    return GVA_SKELETON_ERROR;
}