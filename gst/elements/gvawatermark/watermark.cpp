/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "watermark.h"
#include "config.h"
#include "glib.h"
#include "gva_buffer_map.h"
#include "gva_utils.h"
#include "video_frame.h"
#include <gst/allocators/gstdmabuf.h>
#include <opencv2/opencv.hpp>

static const std::vector<cv::Scalar> color_table_C3 = {
    cv::Scalar(255, 0, 0),   cv::Scalar(0, 255, 0),   cv::Scalar(0, 0, 255),   cv::Scalar(255, 255, 0),
    cv::Scalar(0, 255, 255), cv::Scalar(255, 0, 255), cv::Scalar(255, 170, 0), cv::Scalar(255, 0, 170),
    cv::Scalar(0, 255, 170), cv::Scalar(170, 255, 0), cv::Scalar(170, 0, 255), cv::Scalar(0, 170, 255),
    cv::Scalar(255, 85, 0),  cv::Scalar(85, 255, 0),  cv::Scalar(0, 255, 85),  cv::Scalar(0, 85, 255),
    cv::Scalar(85, 0, 255),  cv::Scalar(255, 0, 85)};

static const std::vector<cv::Scalar> color_table_C1 = {cv::Scalar(0), cv::Scalar(255)};

static cv::Scalar index2color(size_t index, int fourcc) {
    if (fourcc == InferenceBackend::FOURCC_I420 or fourcc == InferenceBackend::FOURCC_NV12) {
        return color_table_C1[index % color_table_C1.size()];
    } else {
        cv::Scalar color = color_table_C3[index % color_table_C3.size()];
        if (fourcc == InferenceBackend::FOURCC_RGBA || fourcc == InferenceBackend::FOURCC_RGBX) {
            std::swap(color[0], color[2]);
        }

        return color;
    }
}

static uint index_for_images = 0;

int Fourcc2OpenCVType(int fourcc) {
    switch (fourcc) {
    case InferenceBackend::FOURCC_NV12:
        return CV_8UC1; // only Y plane
    case InferenceBackend::FOURCC_BGRA:
        return CV_8UC4;
    case InferenceBackend::FOURCC_BGRX:
        return CV_8UC4;
    case InferenceBackend::FOURCC_BGRP:
        return 0;
    case InferenceBackend::FOURCC_BGR:
        return CV_8UC3;
    case InferenceBackend::FOURCC_RGBA:
        return CV_8UC4;
    case InferenceBackend::FOURCC_RGBX:
        return CV_8UC4;
    case InferenceBackend::FOURCC_RGBP:
        return 0;
    case InferenceBackend::FOURCC_I420:
        return CV_8UC1; // only Y plane
    }
    return 0;
}
static void clip_rect(double &x, double &y, double &w, double &h, GstVideoInfo *info) {
    x = (x < 0) ? 0 : (x > info->width) ? info->width : x;
    y = (y < 0) ? 0 : (y > info->height) ? info->height : y;
    w = (w < 0) ? 0 : (x + w > info->width) ? info->width - x : w;
    h = (h < 0) ? 0 : (y + h > info->height) ? info->height - y : h;
}

gboolean draw_label(GstGvaWatermark *gvawatermark, GstBuffer *buffer) {
    void render_human_pose(const GVA::VideoFrame &video_frame, cv::Mat &mat, int format) {
        for (const GVA::Tensor &human_pose_tensor : video_frame.tensors()) {
            if (human_pose_tensor.is_human_pose()) {
                size_t i = 0;
                cv::circle(mat,
                           cv::Point(human_pose_tensor.get_double("nose_x"), human_pose_tensor.get_double("nose_y")), 4,
                           index2color(i++, format), -1);
                cv::circle(mat,
                           cv::Point(human_pose_tensor.get_double("neck_x"), human_pose_tensor.get_double("neck_y")), 4,
                           index2color(i++, format), -1);
                cv::circle(mat,
                           cv::Point(human_pose_tensor.get_double("r_shoulder_x"),
                                     human_pose_tensor.get_double("r_shoulder_y")),
                           4, index2color(i++, format), -1);
                cv::circle(
                    mat,
                    cv::Point(human_pose_tensor.get_double("r_cubit_x"), human_pose_tensor.get_double("r_cubit_y")), 4,
                    index2color(i++, format), -1);
                cv::circle(
                    mat, cv::Point(human_pose_tensor.get_double("r_hand_x"), human_pose_tensor.get_double("r_hand_y")),
                    4, index2color(i++, format), -1);
                cv::circle(mat,
                           cv::Point(human_pose_tensor.get_double("l_shoulder_x"),
                                     human_pose_tensor.get_double("l_shoulder_y")),
                           4, index2color(i++, format), -1);
                cv::circle(
                    mat,
                    cv::Point(human_pose_tensor.get_double("l_cubit_x"), human_pose_tensor.get_double("l_cubit_y")), 4,
                    index2color(i++, format), -1);
                cv::circle(
                    mat, cv::Point(human_pose_tensor.get_double("l_hand_x"), human_pose_tensor.get_double("l_hand_y")),
                    4, index2color(i++, format), -1);
                cv::circle(mat,
                           cv::Point(human_pose_tensor.get_double("r_hip_x"), human_pose_tensor.get_double("r_hip_y")),
                           4, index2color(i++, format), -1);
                cv::circle(
                    mat, cv::Point(human_pose_tensor.get_double("r_knee_x"), human_pose_tensor.get_double("r_knee_y")),
                    4, index2color(i++, format), -1);
                cv::circle(
                    mat, cv::Point(human_pose_tensor.get_double("r_foot_x"), human_pose_tensor.get_double("r_foot_y")),
                    4, index2color(i++, format), -1);
                cv::circle(mat,
                           cv::Point(human_pose_tensor.get_double("l_hip_x"), human_pose_tensor.get_double("l_hip_y")),
                           4, index2color(i++, format), -1);
                cv::circle(
                    mat, cv::Point(human_pose_tensor.get_double("l_knee_x"), human_pose_tensor.get_double("l_knee_y")),
                    4, index2color(i++, format), -1);
                cv::circle(
                    mat, cv::Point(human_pose_tensor.get_double("l_foot_x"), human_pose_tensor.get_double("l_foot_y")),
                    4, index2color(i++, format), -1);
                cv::circle(mat,
                           cv::Point(human_pose_tensor.get_double("r_eye_x"), human_pose_tensor.get_double("r_eye_y")),
                           4, index2color(i++, format), -1);
                cv::circle(mat,
                           cv::Point(human_pose_tensor.get_double("l_eye_x"), human_pose_tensor.get_double("l_eye_y")),
                           4, index2color(i++, format), -1);
                cv::circle(mat,
                           cv::Point(human_pose_tensor.get_double("r_ear_x"), human_pose_tensor.get_double("r_ear_y")),
                           4, index2color(i++, format), -1);
                cv::circle(mat,
                           cv::Point(human_pose_tensor.get_double("l_ear_x"), human_pose_tensor.get_double("l_ear_y")),
                           4, index2color(i++, format), -1);
                cv::putText(
                    mat, std::to_string(human_pose_tensor.get_int("object_id")),
                    cv::Point(human_pose_tensor.get_double("l_ear_x"), human_pose_tensor.get_double("l_ear_y") + 20),
                    cv::FONT_HERSHEY_TRIPLEX, 1, index2color(i++, format), 1);
            }
        }
    }

    void print_points_with_id(GstBuffer * buffer, GstVideoInfo * info) {
        GVA::VideoFrame video_frame(buffer, info);
        std::ofstream person_keypoints_file("person_keypoints_tracking.txt");
        if (person_keypoints_file.is_open()) {
            for (GVA::Tensor &human_pose_tensor : video_frame.tensors()) {
                g_print("pose id %d\n", human_pose_tensor.get_int("pose_id"));
                person_keypoints_file << "pose id " << std::to_string(human_pose_tensor.get_int("pose_id")) << " : ";

                person_keypoints_file << " { nose " << std::to_string(human_pose_tensor.get_double("nose_x")) << " "
                                      << std::to_string(human_pose_tensor.get_double("nose_y")) << "; ";
                person_keypoints_file << "  neck " << std::to_string(human_pose_tensor.get_double("neck_x")) << " "
                                      << std::to_string(human_pose_tensor.get_double("neck_y")) << "; ";
                person_keypoints_file << "  r_shoulder " << std::to_string(human_pose_tensor.get_double("r_shoulder_x"))
                                      << " " << std::to_string(human_pose_tensor.get_double("r_shoulder_y")) << "; ";
                person_keypoints_file << "  r_cubit " << std::to_string(human_pose_tensor.get_double("r_cubit_x"))
                                      << " " << std::to_string(human_pose_tensor.get_double("r_cubit_y")) << "; ";
                person_keypoints_file << "  r_hand " << std::to_string(human_pose_tensor.get_double("r_hand_x")) << " "
                                      << std::to_string(human_pose_tensor.get_double("r_hand_y")) << "; ";
                person_keypoints_file << "  l_shoulder " << std::to_string(human_pose_tensor.get_double("l_shoulder_x"))
                                      << " " << std::to_string(human_pose_tensor.get_double("l_shoulder_y")) << "; ";
                person_keypoints_file << "  l_cubit " << std::to_string(human_pose_tensor.get_double("l_cubit_x"))
                                      << " " << std::to_string(human_pose_tensor.get_double("l_cubit_y")) << "; ";
                person_keypoints_file << "  l_hand " << std::to_string(human_pose_tensor.get_double("l_hand_x")) << " "
                                      << std::to_string(human_pose_tensor.get_double("l_hand_y")) << "; ";
                person_keypoints_file << "  r_hip " << std::to_string(human_pose_tensor.get_double("r_hip_x")) << " "
                                      << std::to_string(human_pose_tensor.get_double("r_hip_y")) << "; ";
                person_keypoints_file << "  r_knee " << std::to_string(human_pose_tensor.get_double("r_knee_x")) << " "
                                      << std::to_string(human_pose_tensor.get_double("r_knee_y")) << "; ";
                person_keypoints_file << "  r_foot " << std::to_string(human_pose_tensor.get_double("r_foot_x")) << " "
                                      << std::to_string(human_pose_tensor.get_double("r_foot_y")) << "; ";
                person_keypoints_file << "  l_hip " << std::to_string(human_pose_tensor.get_double("l_hip_x")) << " "
                                      << std::to_string(human_pose_tensor.get_double("l_hip_y")) << "; ";
                person_keypoints_file << "  l_knee " << std::to_string(human_pose_tensor.get_double("l_knee_x")) << " "
                                      << std::to_string(human_pose_tensor.get_double("l_knee_y")) << "; ";
                person_keypoints_file << "  l_foot " << std::to_string(human_pose_tensor.get_double("l_foot_x")) << " "
                                      << std::to_string(human_pose_tensor.get_double("l_foot_y")) << "; ";
                person_keypoints_file << "  r_eye " << std::to_string(human_pose_tensor.get_double("r_eye_x")) << " "
                                      << std::to_string(human_pose_tensor.get_double("r_eye_y")) << "; ";
                person_keypoints_file << "  l_eye " << std::to_string(human_pose_tensor.get_double("l_eye_x")) << " "
                                      << std::to_string(human_pose_tensor.get_double("l_eye_y")) << "; ";
                person_keypoints_file << "  r_ear " << std::to_string(human_pose_tensor.get_double("r_ear_x")) << " "
                                      << std::to_string(human_pose_tensor.get_double("r_ear_y")) << "; ";
                person_keypoints_file << "  l_ear " << std::to_string(human_pose_tensor.get_double("l_ear_x")) << " "
                                      << std::to_string(human_pose_tensor.get_double("l_ear_y")) << std::endl;
            }
            person_keypoints_file.close();
        }
    }

    void draw_label(GstBuffer * buffer, GstVideoInfo * info) {
        // map GstBuffer to cv::Mat
        InferenceBackend::Image image;
        BufferMapContext mapContext;
        GstMemory *mem = gst_buffer_get_memory(buffer, 0);
        GstMapFlags mapFlags = (mem && gst_is_fd_memory(mem)) ? GST_MAP_READWRITE : GST_MAP_READ; // TODO
        gst_memory_unref(mem);

        try {
            gva_buffer_map(buffer, image, mapContext, &gvawatermark->info, InferenceBackend::MemoryType::SYSTEM,
                           mapFlags);
            auto mapContextPtr = std::unique_ptr<BufferMapContext, std::function<void(BufferMapContext *)>>(
                &mapContext, [&](BufferMapContext *mapContext) { gva_buffer_unmap(buffer, image, *mapContext); });
            int format = Fourcc2OpenCVType(image.format);
            cv::Mat mat(image.height, image.width, format, image.planes[0], gvawatermark->info.stride[0]);

            // construct text labels
            GVA::VideoFrame video_frame(buffer, &gvawatermark->info);
            render_human_pose(video_frame, mat, image.format);

            for (GVA::RegionOfInterest &roi : video_frame.regions()) {
                std::string text = "";
                size_t color_index = roi.label_id();

                auto rect = roi.normalized_rect();
                if (rect.w && rect.h) {
                    rect.x *= gvawatermark->info.width;
                    rect.y *= gvawatermark->info.height;
                    rect.w *= gvawatermark->info.width;
                    rect.h *= gvawatermark->info.height;
                } else {
                    auto rect_u32 = roi.rect();
                    rect = {(double)rect_u32.x, (double)rect_u32.y, (double)rect_u32.w, (double)rect_u32.h};
                }
                clip_rect(rect.x, rect.y, rect.w, rect.h, &gvawatermark->info);

                int object_id = roi.object_id();
                if (object_id > 0) {
                    text = std::to_string(object_id) + ": ";
                    color_index = object_id;
                }

                if (!roi.label().empty()) {
                    if (!text.empty())
                        text += " ";
                    text += roi.label();
                }

                for (GVA::Tensor &tensor : roi.tensors()) {
                    if (!tensor.is_detection()) {
                        std::string label = tensor.label();
                        if (!label.empty()) {
                            if (!text.empty())
                                text += " ";
                            text += label;
                        }
                    }
                    // landmarks rendering
                    if (tensor.model_name().find("landmarks") != std::string::npos ||
                        tensor.format() == "landmark_points") {
                        std::vector<float> data = tensor.data<float>();
                        for (guint i = 0; i < data.size() / 2; i++) {
                            cv::Scalar color = index2color(i, image.format);
                            int x_lm = rect.x + rect.w * data[2 * i];
                            int y_lm = rect.y + rect.h * data[2 * i + 1];
                            cv::circle(mat, cv::Point(x_lm, y_lm), 1 + static_cast<int>(0.012 * rect.w), color, -1);
                        }
                    }
                }

                // draw rectangle
                cv::Scalar color = index2color(color_index, image.format); // TODO: Is it good mapping to colors?

                cv::Point2f bbox_min(rect.x, rect.y);
                cv::Point2f bbox_max(rect.x + rect.w, rect.y + rect.h);
                cv::rectangle(mat, bbox_min, bbox_max, color, 1);

                // put text
                cv::Point2f pos(rect.x, rect.y - 5.f);
                if (pos.y < 0)
                    pos.y = rect.y + 30.f;
                cv::putText(mat, text, pos, cv::FONT_HERSHEY_TRIPLEX, 1, color, 1);
                // std::string path =
                //     "/home/pbochenk/projects/diplom/video-samples/gvaskeleton_images_with_cv_clear/frame" +
                //     std::to_string(index_for_images++) + ".jpg";
                // cv::imwrite(path, mat);
            }
        } catch (const std::exception &e) {
            GST_ELEMENT_ERROR(gvawatermark, STREAM, FAILED, ("watermark has failed to draw label"),
                              ("%s", CreateNestedErrorMsg(e).c_str()));
            return FALSE;
        }
        return TRUE;
    }
