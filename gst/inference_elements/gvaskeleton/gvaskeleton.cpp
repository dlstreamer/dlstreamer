#include "gvaskeleton.h"

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

// void convertPoses2Array(const std::vector<HumanPose> &poses, float *data, size_t kp_num) {
//     size_t i = 0;
//     for (const auto &pose : poses) {
//         for (const auto &keypoint : pose.keypoints) {
//             if (i >= kp_num) {
//                 std::runtime_error("Number of keypoints bigger then data's size");
//             }
//             data[i++] = keypoint.x;
//             data[i++] = keypoint.y;
//         }
//     }
// }
// void copy_buffer_to_structure(GstStructure *structure, const void *buffer, int size) {
//     ITT_TASK(__FUNCTION__);
//     GVariant *v = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, buffer, size, 1);
//     gsize n_elem;
//     gst_structure_set(structure, "data_buffer", G_TYPE_VARIANT, v, "data", G_TYPE_POINTER,
//                       g_variant_get_fixed_array(v, &n_elem, 1), NULL);
// }

// GvaSkeletonStatus attach_poses_to_buffer(const std::vector<HumanPose> &poses, GstBuffer *buf) {
//     try {
//         size_t kp_num = 0;
//         for (const auto &pose : poses)
//             kp_num += pose.keypoints.size() * 2;

//         float *data = new float[kp_num];
//         convertPoses2Array(poses, data, kp_num);

//         GstStructure *hp_struct;
//         gst_structure_new_empty(hp_struct, "human_poses_keypoints");

//         delete[] data;
//         return GVA_SKELETON_OK;
//     } catch (const std::exception &e) {
//         GVA_ERROR(e.what());
//     }
//     return GVA_SKELETON_ERROR;
// }

GvaSkeletonStatus hpe_to_estimate(HumanPoseEstimator *hpe_obj, GstBuffer *buf, GstVideoInfo *info) {
    try {
        InferenceBackend::Image image;

        BufferMapContext mapContext;
        GstMemory *mem = gst_buffer_get_memory(buf, 0);
        GstMapFlags mapFlags = (mem && gst_is_fd_memory(mem)) ? GST_MAP_READWRITE : GST_MAP_READ; // TODO
        gst_memory_unref(mem);
        //  should be unmapped with gst_buffer_unmap() after usage
        gva_buffer_map(buf, image, mapContext, info, InferenceBackend::MemoryType::SYSTEM, mapFlags);
        int format = Fourcc2OpenCVType(image.format);

        cv::Mat mat(image.height, image.width, format, image.planes[0], info->stride[0]);
        // const cv::Mat mat(image.height, image.width, format, image.planes[0], info->stride[0]);

        if (mat.empty())
            throw std::runtime_error("Uppsss... Preproc has not happend.");

        std::vector<HumanPose> poses = hpe_obj->estimate(mat);

        

        // if (attach_poses_to_buffer(poses, buf) == GVA_SKELETON_ERROR)
        //     throw std::runtime_error("Uppsss... Postproc has not happend.");

        // TODO: move it to gvawatermark and return const for mat
        renderHumanPose(poses, mat);

        gva_buffer_unmap(buf, image, mapContext);

        return GVA_SKELETON_OK;
    } catch (const std::exception &e) {
        GVA_ERROR(e.what());
    }
    return GVA_SKELETON_ERROR;
}