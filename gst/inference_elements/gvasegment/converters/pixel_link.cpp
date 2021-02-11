/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "converters/pixel_link.h"

#include "copy_blob_to_gststruct.h"
#include "gstgvasegment.h"
#include "inference_backend/logger.h"
#include "video_frame.h"

#include <cassert>

using namespace SegmentationPlugin;
using namespace Converters;

bool PixelLinkConverter::OutputLayersName::checkBlobCorrectness(
    const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs) {
    if (!are_valid_layers_names) {
        if (!output_blobs.count(link_logits))
            throw std::invalid_argument("OutputBlob must contain \"" + link_logits + "\" layer");
        if (!output_blobs.count(segm_logits))
            throw std::invalid_argument("OutputBlob must contain \"" + segm_logits + "\" layer");
        are_valid_layers_names = true;
    }
    return are_valid_layers_names;
}

PixelLinkConverter::PixelLinkConverter(double cls_conf_threshold, double link_conf_threshold, int show_zero_class) {
    this->cls_conf_threshold = cls_conf_threshold;
    this->link_conf_threshold = link_conf_threshold;
    this->show_zero_class = show_zero_class;
}

// Copied from OpenVinoToolkit demos
// https://github.com/openvinotoolkit/open_model_zoo/blob/master/demos/text_detection_demo/src/text_detection.cpp

namespace {
void softmax(std::vector<float> *data) {
    auto &rdata = *data;
    const size_t last_dim = 2;
    for (size_t i = 0; i < rdata.size(); i += last_dim) {
        float m = std::max(rdata[i], rdata[i + 1]);
        rdata[i] = std::exp(rdata[i] - m);
        rdata[i + 1] = std::exp(rdata[i + 1] - m);
        float s = rdata[i] + rdata[i + 1];
        rdata[i] /= s;
        rdata[i + 1] /= s;
    }
}

std::vector<float> transpose4d(const std::vector<float> &data, const std::vector<size_t> &shape,
                               const std::vector<size_t> &axes) {
    if (shape.size() != axes.size())
        throw std::runtime_error("Shape and axes must have the same dimension.");

    for (size_t a : axes) {
        if (a >= shape.size())
            throw std::runtime_error("Axis must be less than dimension of shape.");
    }

    size_t total_size = shape[0] * shape[1] * shape[2] * shape[3];

    std::vector<size_t> steps{shape[axes[1]] * shape[axes[2]] * shape[axes[3]], shape[axes[2]] * shape[axes[3]],
                              shape[axes[3]], 1};

    size_t source_data_idx = 0;
    std::vector<float> new_data(total_size, 0);

    std::vector<size_t> ids(shape.size());
    for (ids[0] = 0; ids[0] < shape[0]; ids[0]++) {
        for (ids[1] = 0; ids[1] < shape[1]; ids[1]++) {
            for (ids[2] = 0; ids[2] < shape[2]; ids[2]++) {
                for (ids[3] = 0; ids[3] < shape[3]; ids[3]++) {
                    size_t new_data_idx = ids[axes[0]] * steps[0] + ids[axes[1]] * steps[1] + ids[axes[2]] * steps[2] +
                                          ids[axes[3]] * steps[3];
                    new_data[new_data_idx] = data[source_data_idx++];
                }
            }
        }
    }
    return new_data;
}

std::vector<size_t> ieSizeToVector(const std::vector<size_t> &ie_output_dims) {
    std::vector<size_t> blob_sizes(ie_output_dims.size(), 0);
    for (size_t i = 0; i < blob_sizes.size(); ++i) {
        blob_sizes[i] = ie_output_dims[i];
    }
    return blob_sizes;
}

std::vector<float> sliceAndGetSecondChannel(const std::vector<float> &data) {
    std::vector<float> new_data(data.size() / 2, 0);
    for (size_t i = 0; i < data.size() / 2; i++) {
        new_data[i] = data[2 * i + 1];
    }
    return new_data;
}

std::vector<cv::RotatedRect> maskToBoxes(cv::Mat &mask, float min_area, float min_height, cv::Size image_size) {
    std::vector<cv::RotatedRect> bboxes;
    double min_val;
    double max_val;
    cv::minMaxLoc(mask, &min_val, &max_val);
    int max_bbox_idx = static_cast<int>(max_val);
    cv::Mat resized_mask;
    cv::resize(mask, resized_mask, image_size, 0, 0, cv::INTER_NEAREST);
    cv::Mat zero_mask(mask.rows, mask.cols, CV_32SC1, cv::Scalar(0));

    for (int i = 1; i <= max_bbox_idx; i++) {
        cv::Mat bbox_mask = resized_mask == i;
        std::vector<std::vector<cv::Point>> contours;

        cv::findContours(bbox_mask, contours, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);
        if (contours.empty())
            continue;
        cv::RotatedRect r = cv::minAreaRect(contours[0]);
        if (std::min(r.size.width, r.size.height) < min_height || r.size.area() < min_area) {
            mask.setTo(0, mask == i);
            continue;
        }
        bboxes.emplace_back(r);
    }

    return bboxes;
}

int findRoot(int point, std::unordered_map<int, int> *group_mask) {
    int root = point;
    bool update_parent = false;
    while (group_mask->at(root) != -1) {
        root = group_mask->at(root);
        update_parent = true;
    }
    if (update_parent) {
        (*group_mask)[point] = root;
    }
    return root;
}

void join(int p1, int p2, std::unordered_map<int, int> *group_mask) {
    int root1 = findRoot(p1, group_mask);
    int root2 = findRoot(p2, group_mask);
    if (root1 != root2) {
        (*group_mask)[root1] = root2;
    }
}

cv::Mat get_all(const std::vector<cv::Point> &points, int w, int h, std::unordered_map<int, int> *group_mask) {
    std::unordered_map<int, int> root_map;

    cv::Mat mask(h, w, CV_32SC1, cv::Scalar(0));
    for (const auto &point : points) {
        int point_root = findRoot(point.x + point.y * w, group_mask);
        if (root_map.find(point_root) == root_map.end()) {
            root_map.emplace(point_root, static_cast<int>(root_map.size() + 1));
        }
        mask.at<int>(point.x + point.y * w) = root_map[point_root];
    }

    return mask;
}

cv::Mat decodeImageByJoin(const std::vector<float> &cls_data, const std::vector<int> &cls_data_shape,
                          const std::vector<float> &link_data, const std::vector<int> &link_data_shape,
                          float cls_conf_threshold, float link_conf_threshold) {
    int h = cls_data_shape[1];
    int w = cls_data_shape[2];

    std::vector<uchar> pixel_mask(h * w, 0);
    std::unordered_map<int, int> group_mask;
    std::vector<cv::Point> points;
    for (size_t i = 0; i < pixel_mask.size(); i++) {
        pixel_mask[i] = cls_data[i] >= cls_conf_threshold;
        if (pixel_mask[i]) {
            points.emplace_back(i % w, i / w);
            group_mask[i] = -1;
        }
    }

    std::vector<uchar> link_mask(link_data.size(), 0);
    for (size_t i = 0; i < link_mask.size(); i++) {
        link_mask[i] = link_data[i] >= link_conf_threshold;
    }

    size_t neighbours = size_t(link_data_shape[3]);
    for (const auto &point : points) {
        size_t neighbour = 0;
        for (int ny = point.y - 1; ny <= point.y + 1; ny++) {
            for (int nx = point.x - 1; nx <= point.x + 1; nx++) {
                if (nx == point.x && ny == point.y)
                    continue;
                if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                    uchar pixel_value = pixel_mask[size_t(ny) * size_t(w) + size_t(nx)];
                    uchar link_value =
                        link_mask[(size_t(point.y) * size_t(w) + size_t(point.x)) * neighbours + neighbour];
                    if (pixel_value && link_value) {
                        join(point.x + point.y * w, nx + ny * w, &group_mask);
                    }
                }
                neighbour++;
            }
        }
    }

    return get_all(points, w, h, &group_mask);
}
} // namespace

bool PixelLinkConverter::process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                                 const std::vector<std::shared_ptr<InferenceFrame>> &frames,
                                 const std::string &model_name, const std::string &layer_name, GValueArray *,
                                 GstStructure *segmentation_result) {
    ITT_TASK(__FUNCTION__);
    bool flag = false;
    try {
        if (not segmentation_result)
            throw std::invalid_argument("segmentation_result tensor is nullptr");

        // Check layer existence and correctness
        layers_name.checkBlobCorrectness(output_blobs);

        // Segm_logits layer -> FP32
        const auto &segm_logits_iter = output_blobs.find(layers_name.segm_logits);
        const auto &segm_logits_blob = segm_logits_iter->second;
        if (not segm_logits_blob)
            throw std::invalid_argument("Output blob is nullptr");
        if (segm_logits_blob->GetPrecision() != InferenceBackend::OutputBlob::Precision::FP32)
            throw std::invalid_argument("\"" + layers_name.segm_logits + "\" layer should have FP32 precision");
        const float *segm_logits_data = (const float *)segm_logits_blob->GetData();
        if (not segm_logits_data)
            throw std::invalid_argument("Output blob data is nullptr");
        auto &segm_logits_dims = segm_logits_blob->GetDims();
        guint segm_logits_dims_size = segm_logits_dims.size();
        if (segm_logits_dims_size != 4)
            throw std::invalid_argument("Output \"" + layers_name.segm_logits +
                                        "\" Blob must have dimentions size 4 but has dimentions size " +
                                        std::to_string(segm_logits_dims_size));

        // link_logits layer -> FP32
        const auto &link_logits_iter = output_blobs.find(layers_name.link_logits);
        const auto &link_logits_blob = link_logits_iter->second;
        if (not link_logits_blob)
            throw std::invalid_argument("Output blob is nullptr");
        if (link_logits_blob->GetPrecision() != InferenceBackend::OutputBlob::Precision::FP32)
            throw std::invalid_argument("\"" + layers_name.link_logits + "\" layer should have FP32 precision");
        const float *link_logits_data = (const float *)link_logits_blob->GetData();
        if (not link_logits_data)
            throw std::invalid_argument("Output blob data is nullptr");
        auto &link_logits_dims = link_logits_blob->GetDims();
        guint link_logits_dims_size = link_logits_dims.size();
        if (link_logits_dims_size != 4)
            throw std::invalid_argument("Output \"" + layers_name.link_logits +
                                        "\" Blob must have dimentions size 4 but has dimentions size " +
                                        std::to_string(link_logits_dims_size));

        if (link_logits_dims[2] != segm_logits_dims[2] || link_logits_dims[3] != segm_logits_dims[3])
            throw std::invalid_argument("Output height and width shold be the same");

        if (link_logits_dims[1] != 16 || segm_logits_dims[1] != 2)
            throw std::invalid_argument("Invalid dims[1] value");

        size_t frame_id = 0;
        if (frame_id >= frames.size())
            return flag;

        GstVideoInfo video_info;

        if (frames[frame_id]->gva_base_inference->info) {
            video_info = *frames[frame_id]->gva_base_inference->info;
        } else {
            video_info.width = frames[frame_id]->roi.w;
            video_info.height = frames[frame_id]->roi.h;
        }
        GVA::VideoFrame video_frame(frames[frame_id]->buffer, frames[frame_id]->info);

        const int kMinArea = 300;
        const int kMinHeight = 10;

        size_t link_data_size = link_logits_dims[0] * link_logits_dims[1] * link_logits_dims[2] * link_logits_dims[3];
        std::vector<float> link_data(link_logits_data, link_logits_data + link_data_size);
        link_data = transpose4d(link_data, ieSizeToVector(link_logits_dims), {0, 2, 3, 1});
        softmax(&link_data);
        link_data = sliceAndGetSecondChannel(link_data);
        std::vector<int> new_link_data_shape(4);
        new_link_data_shape[0] = static_cast<int>(link_logits_dims[0]);
        new_link_data_shape[1] = static_cast<int>(link_logits_dims[2]);
        new_link_data_shape[2] = static_cast<int>(link_logits_dims[3]);
        new_link_data_shape[3] = static_cast<int>(link_logits_dims[1]) / 2;

        size_t cls_data_size = segm_logits_dims[0] * segm_logits_dims[1] * segm_logits_dims[2] * segm_logits_dims[3];
        std::vector<float> cls_data(segm_logits_data, segm_logits_data + cls_data_size);
        cls_data = transpose4d(cls_data, ieSizeToVector(segm_logits_dims), {0, 2, 3, 1});
        softmax(&cls_data);
        cls_data = sliceAndGetSecondChannel(cls_data);
        std::vector<int> new_cls_data_shape(4);
        new_cls_data_shape[0] = static_cast<int>(segm_logits_dims[0]);
        new_cls_data_shape[1] = static_cast<int>(segm_logits_dims[2]);
        new_cls_data_shape[2] = static_cast<int>(segm_logits_dims[3]);
        new_cls_data_shape[3] = static_cast<int>(segm_logits_dims[1]) / 2;

        cv::Size image_size(video_info.width, video_info.height);
        cv::Mat mask = decodeImageByJoin(cls_data, new_cls_data_shape, link_data, new_link_data_shape,
                                         cls_conf_threshold, link_conf_threshold);
        std::vector<cv::RotatedRect> rects =
            maskToBoxes(mask, static_cast<float>(kMinArea), static_cast<float>(kMinHeight), image_size);

        GVA::Tensor tensor = video_frame.add_tensor();

        tensor.set_int("show_zero_class", show_zero_class);

        GstStructure *tensor_structure = tensor.gst_structure();
        gst_structure_set_name(tensor_structure, "semantic_segmentation");

        // make sure name="semantic_segmentation"
        assert(gst_structure_has_name(tensor_structure, "semantic_segmentation"));

        // Bounding boxes post proc
        for (auto &box : rects) {
            float w = box.size.width;
            float h = box.size.height;
            float angle = -M_PI * box.angle / 180;
            std::vector<cv::Point2f> points = {{w / 2, h / 2}, {w / 2, -h / 2}, {-w / 2, h / 2}, {-w / 2, -h / 2}};

            // Boxes rotation
            for (auto &p : points)
                p = {static_cast<float>(p.x * cos(angle) - p.y * sin(angle)),
                     static_cast<float>(p.x * sin(angle) + p.y * cos(angle))};

            float x_min = std::max(box.center.x + std::min({points[0].x, points[1].x, points[2].x, points[3].x}), 0.f);
            float x_max = std::min(box.center.x + std::max({points[0].x, points[1].x, points[2].x, points[3].x}),
                                   (float)video_info.width);
            float y_min = std::max(box.center.y + std::min({points[0].y, points[1].y, points[2].y, points[3].y}), 0.f);
            float y_max = std::min(box.center.y + std::max({points[0].y, points[1].y, points[2].y, points[3].y}),
                                   (float)video_info.height);

            video_frame.add_region(x_min, y_min, x_max - x_min, y_max - y_min, std::string(), 1, false);
        }

        copySemanticInfoToGstStructure((const float *)mask.data, segm_logits_dims, model_name, layer_name,
                                       InferenceBackend::OutputBlob::Precision::I32, segm_logits_blob->GetLayout(),
                                       frames.size(), frame_id, tensor_structure);
        flag = true;
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do SemanticDefault post-processing"));
    }
    return flag;
}
