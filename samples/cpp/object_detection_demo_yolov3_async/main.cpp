/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 * object_detection_demo_yolov3_async implementation based on object_detection_demo_yolov3_async
 * See https://github.com/opencv/open_model_zoo/tree/2018/demos/object_detection_demo_yolov3_async
 * Differences:
 * ParseYOLOV3Output function changed for working with gva tensors
 * Adapted code style to match with Video Analytics GStreamer* plugins project
 ******************************************************************************/

#include <dirent.h>
#include <gio/gio.h>
#include <gst/gst.h>
#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <stdlib.h>

#include "coco_labels.h"
#include "gva_tensor_meta.h"
#include "video_frame.h"

#define UNUSED(x) (void)(x)

std::vector<std::string> SplitString(const std::string input, char delimiter = ':') {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(input);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

void ExploreDir(std::string search_dir, const std::string &model_name, std::vector<std::string> &result) {
    if (auto dir_handle = opendir(search_dir.c_str())) {
        while (auto file_handle = readdir(dir_handle)) {
            if ((!file_handle->d_name) || (file_handle->d_name[0] == '.'))
                continue;
            if (file_handle->d_type == DT_DIR)
                ExploreDir(search_dir + file_handle->d_name + "/", model_name, result);
            if (file_handle->d_type == DT_REG) {
                std::string name(file_handle->d_name);
                if (name == model_name)
                    result.push_back(search_dir + "/" + name);
            }
        }
        closedir(dir_handle);
    }
}

std::vector<std::string> FindModel(const std::vector<std::string> &search_dirs, const std::string &model_name) {
    std::vector<std::string> result = {};
    for (const std::string &dir : search_dirs) {
        ExploreDir(dir + "/", model_name, result);
    }
    return result;
}

std::string to_upper_case(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}

std::map<std::string, std::string> FindModels(const std::vector<std::string> &search_dirs,
                                              const std::vector<std::string> &model_names,
                                              const std::string &precision) {
    std::map<std::string, std::string> result;
    for (const std::string &model_name : model_names) {
        std::vector<std::string> model_paths = FindModel(search_dirs, model_name);
        if (model_paths.empty())
            continue;
        result[model_name] = model_paths.front();
        for (const auto &model_path : model_paths)
            // TODO extract precision from xml file
            if (to_upper_case(model_path).find(to_upper_case(precision)) != std::string::npos) {
                result[model_name] = model_path;
                break;
            }
    }
    if (result.empty())
        throw std::runtime_error("Can't find file for model");
    return result;
}

const std::string env_models_path =
    std::string() + (getenv("MODELS_PATH") != NULL
                         ? getenv("MODELS_PATH")
                         : getenv("INTEL_CVSDK_DIR") != NULL
                               ? std::string() + getenv("INTEL_CVSDK_DIR") + "/deployment_tools/intel_models" + "/"
                               : "");

const std::vector<std::string> default_detection_model_names = {"yolov3.xml", "frozen_darknet_yolov3_model.xml"};

gchar *input_file = NULL;
gchar const *detection_model = NULL;

gchar *extension = NULL;
gchar const *device = "CPU";
gchar const *model_precision = "FP32";
gint batch_size = 1;
gdouble threshold = 0.4;
gboolean no_display = FALSE;

static GOptionEntry opt_entries[] = {
    {"input", 'i', 0, G_OPTION_ARG_STRING, &input_file, "Path to input video file", NULL},
    {"precision", 'p', 0, G_OPTION_ARG_STRING, &model_precision, "Models precision. Default: FP32", NULL},
    {"detection", 'm', 0, G_OPTION_ARG_STRING, &detection_model, "Path to detection model file", NULL},
    {"extension", 'e', 0, G_OPTION_ARG_STRING, &extension, "Path to custom layers extension library", NULL},
    {"device", 'd', 0, G_OPTION_ARG_STRING, &device, "Device to run inference", NULL},
    {"batch", 'b', 0, G_OPTION_ARG_INT, &batch_size, "Batch size", NULL},
    {"threshold", 't', 0, G_OPTION_ARG_DOUBLE, &threshold, "Confidence threshold for detection (0 - 1)", NULL},
    {"no-display", 'n', 0, G_OPTION_ARG_NONE, &no_display, "Run without display", NULL},
    GOptionEntry()};

struct DetectionObject {
    int xmin, ymin, xmax, ymax, class_id;
    float confidence;

    DetectionObject(double x, double y, double h, double w, int class_id, float confidence, float h_scale,
                    float w_scale) {
        this->xmin = static_cast<int>((x - w / 2) * w_scale);
        this->ymin = static_cast<int>((y - h / 2) * h_scale);
        this->xmax = static_cast<int>(this->xmin + w * w_scale);
        this->ymax = static_cast<int>(this->ymin + h * h_scale);
        this->class_id = class_id;
        this->confidence = confidence;
    }

    bool operator<(const DetectionObject &s2) const {
        return this->confidence < s2.confidence;
    }
};

double IntersectionOverUnion(const DetectionObject &box_1, const DetectionObject &box_2) {
    double width_of_overlap_area = fmin(box_1.xmax, box_2.xmax) - fmax(box_1.xmin, box_2.xmin);
    double height_of_overlap_area = fmin(box_1.ymax, box_2.ymax) - fmax(box_1.ymin, box_2.ymin);
    double area_of_overlap;
    if (width_of_overlap_area < 0 || height_of_overlap_area < 0)
        area_of_overlap = 0;
    else
        area_of_overlap = width_of_overlap_area * height_of_overlap_area;
    double box_1_area = (box_1.ymax - box_1.ymin) * (box_1.xmax - box_1.xmin);
    double box_2_area = (box_2.ymax - box_2.ymin) * (box_2.xmax - box_2.xmin);
    double area_of_union = box_1_area + box_2_area - area_of_overlap;
    return area_of_overlap / area_of_union;
}

static int EntryIndex(int side, int lcoords, int lclasses, int location, int entry) {
    int n = location / (side * side);
    int loc = location % (side * side);
    return n * side * side * (lcoords + lclasses + 1) + entry * side * side + loc;
}

void ParseYOLOV3Output(GVA::Tensor &tensor_yolo, int image_width, int image_height,
                       std::vector<DetectionObject> &objects) {
    const int out_blob_h = tensor_yolo.dims()[2];
    constexpr int coords = 4;
    constexpr int num = 3;
    constexpr int classes = 80;
    const std::vector<float> anchors = {10.0, 13.0, 16.0,  30.0,  33.0, 23.0,  30.0,  61.0,  62.0,
                                        45.0, 59.0, 119.0, 116.0, 90.0, 156.0, 198.0, 373.0, 326.0};
    int side = out_blob_h;
    int anchor_offset = 0;
    switch (side) {
    case 13:
        anchor_offset = 2 * 6;
        break;
    case 26:
        anchor_offset = 2 * 3;
        break;
    case 52:
        anchor_offset = 2 * 0;
        break;
    default:
        throw std::runtime_error("Invalid output size");
    }
    int side_square = side * side;

    std::vector<float> output_blob = tensor_yolo.data<float>();
    for (int i = 0; i < side_square; ++i) {
        int row = i / side;
        int col = i % side;
        for (int n = 0; n < num; ++n) {

            int obj_index = EntryIndex(side, coords, classes, n * side * side + i, coords);
            int box_index = EntryIndex(side, coords, classes, n * side * side + i, 0);

            float scale = output_blob[obj_index];
            if (scale < threshold)
                continue;
            double x = (col + output_blob[box_index + 0 * side_square]) / side * 416;
            double y = (row + output_blob[box_index + 1 * side_square]) / side * 416;

            double width = std::exp(output_blob[box_index + 2 * side_square]) * anchors[anchor_offset + 2 * n];
            double height = std::exp(output_blob[box_index + 3 * side_square]) * anchors[anchor_offset + 2 * n + 1];

            for (int j = 0; j < classes; ++j) {
                int class_index = EntryIndex(side, coords, classes, n * side_square + i, coords + 1 + j);
                float prob = scale * output_blob[class_index];
                if (prob < threshold)
                    continue;
                DetectionObject obj(x, y, height, width, j, prob,
                                    static_cast<float>(image_height) / static_cast<float>(416),
                                    static_cast<float>(image_width) / static_cast<float>(416));
                objects.push_back(obj);
            }
        }
    }
}

void DrawObjects(std::vector<DetectionObject> &objects, cv::Mat &frame) {
    std::sort(objects.begin(), objects.end());
    for (size_t i = 0; i < objects.size(); ++i) {
        if (objects[i].confidence == 0)
            continue;
        for (size_t j = i + 1; j < objects.size(); ++j)
            if (IntersectionOverUnion(objects[i], objects[j]) >= 0.4)
                objects[j].confidence = 0;
    }
    // Drawing boxes
    for (const auto &object : objects) {
        if (object.confidence < 0)
            continue;
        guint label = object.class_id;
        float confidence = object.confidence;

        if (confidence > 0) {
            /** Drawing only objects when >confidence_threshold probability **/
            std::ostringstream conf;
            conf << ":" << std::fixed << std::setprecision(3) << confidence;
            cv::putText(
                frame,
                (label < labels.size() ? labels[label] : std::string("label #") + std::to_string(label)) + conf.str(),
                cv::Point2f(object.xmin, object.ymin - 5), cv::FONT_HERSHEY_COMPLEX_SMALL, 1, cv::Scalar(0, 0, 255));
            cv::rectangle(frame, cv::Point2f(object.xmin, object.ymin), cv::Point2f(object.xmax, object.ymax),
                          cv::Scalar(0, 0, 255));
        }
    }
}

// This structure will be used to pass user data (such as memory type) to the callback function.

static GstPadProbeReturn pad_probe_callback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
    UNUSED(user_data);

    auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);

    if (buffer == NULL)
        return GST_PAD_PROBE_OK;

    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (!caps)
        throw std::runtime_error("Can't get current caps");

    GVA::VideoFrame video_frame(buffer, caps);
    gint width = video_frame.video_info()->width;
    gint height = video_frame.video_info()->height;

    // Map buffer and create OpenCV image
    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ))
        return GST_PAD_PROBE_OK;
    cv::Mat mat(height, width, CV_8UC4, map.data);

    // Parse and draw outputs
    std::vector<DetectionObject> objects;
    for (GVA::Tensor &tensor : video_frame.tensors()) {
        if (tensor.model_name().find("yolov3") != std::string::npos) {
            ParseYOLOV3Output(tensor, width, height, objects);
        }
    }
    DrawObjects(objects, mat);
    GST_PAD_PROBE_INFO_DATA(info) = buffer;

    gst_buffer_unmap(buffer, &map);
    gst_caps_unref(caps);
    return GST_PAD_PROBE_OK;
}

int main(int argc, char *argv[]) {
    // Parse arguments
    GOptionContext *context = g_option_context_new("sample");
    g_option_context_add_main_entries(context, opt_entries, "sample");
    g_option_context_add_group(context, gst_init_get_option_group());
    GError *error = NULL;
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_print("option parsing failed: %s\n", error->message);
        return 1;
    }
    if (!input_file) {
        g_print("Please specify input file:\n%s\n", g_option_context_get_help(context, TRUE, NULL));
        return 1;
    }

    if (env_models_path.empty()) {
        throw std::runtime_error("Enviroment variable MODELS_PATH is not set");
    }
    if (detection_model == NULL) {
        std::map<std::string, std::string> model_paths =
            FindModels(SplitString(env_models_path), default_detection_model_names, model_precision);
        for (const std::string &default_model_name : default_detection_model_names) {
            if (!model_paths[default_model_name].empty())
                detection_model = g_strdup(model_paths[default_model_name].c_str());
        }
    }

    if (!detection_model) {
        g_print("Please specify detection model path:\n%s\n", g_option_context_get_help(context, TRUE, NULL));
        return 1;
    }

    gchar const *preprocess_pipeline = "decodebin ! videoconvert n-threads=4 ! videoscale n-threads=4 ";
    gchar const *capfilter = "video/x-raw,format=BGRA";
    gchar const *sink = no_display ? "identity signal-handoffs=false ! fakesink sync=false"
                                   : "fpsdisplaysink video-sink=xvimagesink sync=false";

    // Build the pipeline
    auto launch_str =
        g_strdup_printf("filesrc location=%s ! %s ! capsfilter caps=\"%s\" ! "
                        "gvainference name=gvadetect model=%s device=%s batch-size=%d ! queue ! "
                        "videoconvert n-threads=4 ! %s ",
                        input_file, preprocess_pipeline, capfilter, detection_model, device, batch_size, sink);
    g_print("PIPELINE: %s \n", launch_str);
    GstElement *pipeline = gst_parse_launch(launch_str, NULL);
    g_free(launch_str);

    // set probe callback
    GstElement *gvadetect = gst_bin_get_by_name(GST_BIN(pipeline), "gvadetect");
    GstPad *pad = gst_element_get_static_pad(gvadetect, "src");
    gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, pad_probe_callback, NULL, NULL);
    gst_object_unref(pad);

    // Start playing
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    // Wait until error or EOS
    GstBus *bus = gst_element_get_bus(pipeline);

    int ret_code = 0;

    GstMessage *msg = gst_bus_poll(bus, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS), -1);

    if (msg && GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        GError *err = NULL;
        gchar *dbg_info = NULL;

        gst_message_parse_error(msg, &err, &dbg_info);
        g_printerr("ERROR from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
        g_printerr("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");

        g_error_free(err);
        g_free(dbg_info);
        ret_code = -1;
    }

    if (msg)
        gst_message_unref(msg);

    // Free resources
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return ret_code;
}
