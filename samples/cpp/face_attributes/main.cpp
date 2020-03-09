/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <algorithm>
#include <dirent.h>
#include <gio/gio.h>
#include <gst/gst.h>
#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <stdlib.h>

#include "draw_axes.h"
#include "video_frame.h"

using namespace std;

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
    for (std::string dir : search_dirs) {
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
    for (std::string model_name : model_names) {
        std::vector<std::string> model_paths = FindModel(search_dirs, model_name);
        if (model_paths.empty())
            throw std::runtime_error("Can't find file for model: " + model_name);
        result[model_name] = model_paths.front();
        for (auto &model_path : model_paths)
            // TODO extract precision from xml file
            if (to_upper_case(model_path).find(to_upper_case(precision)) != std::string::npos) {
                result[model_name] = model_path;
                break;
            }
    }
    return result;
}

const std::string env_models_path =
    std::string() + (getenv("MODELS_PATH") != NULL
                         ? getenv("MODELS_PATH")
                         : getenv("INTEL_CVSDK_DIR") != NULL
                               ? std::string() + getenv("INTEL_CVSDK_DIR") + "/deployment_tools/intel_models" + "/"
                               : "");

const std::vector<std::string> default_detection_model_names = {"face-detection-adas-0001.xml"};

const std::vector<std::string> default_classification_model_names = {
    "facial-landmarks-35-adas-0002.xml", "age-gender-recognition-retail-0013.xml",
    "emotions-recognition-retail-0003.xml", "head-pose-estimation-adas-0001.xml"};

gchar const *detection_model = NULL;
gchar const *classification_models = NULL;

gchar *input_file = NULL;
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
    {"classification", 'c', 0, G_OPTION_ARG_STRING, &classification_models,
     "Path to classification models as ',' separated list", NULL},
    {"extension", 'e', 0, G_OPTION_ARG_STRING, &extension, "Path to custom layers extension library", NULL},
    {"device", 'd', 0, G_OPTION_ARG_STRING, &device, "Device to run inference", NULL},
    {"batch", 'b', 0, G_OPTION_ARG_INT, &batch_size, "Batch size", NULL},
    {"threshold", 't', 0, G_OPTION_ARG_DOUBLE, &threshold, "Confidence threshold for detection (0 - 1)", NULL},
    {"no-display", 'n', 0, G_OPTION_ARG_NONE, &no_display, "Run without display", NULL},
    GOptionEntry()};

// This structure will be used to pass user data (such as memory type) to the callback function.

static GstPadProbeReturn pad_probe_callback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
    UNUSED(user_data);

    auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);

    // Making a buffer writable can fail (for example if it cannot be copied and is used more than once)
    // buffer = gst_buffer_make_writable(buffer);
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

    // Iterate detected objects and all attributes
    std::vector<GVA::RegionOfInterest> regions = video_frame.regions();
    for (GVA::RegionOfInterest &roi : regions) {
        auto meta = roi.meta();
        string label;
        float head_angle_r = 0, head_angle_p = 0, head_angle_y = 0;
        for (auto tensor : roi) {
            string model_name = tensor.model_name();
            string layer_name = tensor.layer_name();
            vector<float> data = tensor.data<float>();
            if (model_name.find("landmarks") != string::npos) {
                static const auto lm_color = cv::Scalar(0, 255, 255);
                for (guint i = 0; i < data.size() / 2; i++) {
                    int x_lm = meta->x + meta->w * data[2 * i];
                    int y_lm = meta->y + meta->h * data[2 * i + 1];
                    cv::circle(mat, cv::Point(x_lm, y_lm), 1 + static_cast<int>(0.012 * meta->w), lm_color, -1);
                }
            }
            if (model_name.find("gender") != string::npos && layer_name.find("prob") != string::npos) {
                label += (data[1] > 0.5) ? " M " : " F ";
            }
            if (layer_name.find("age") != string::npos) {
                label += to_string((int)(data[0] * 100));
            }
            if (model_name.find("EmoNet") != string::npos) {
                static const vector<string> emotionsDesc = {"neutral", "happy", "sad", "surprise", "anger"};
                int index = max_element(begin(data), end(data)) - begin(data);
                label += " " + emotionsDesc[index];
            }
            if (layer_name.find("angle_r") != string::npos) {
                head_angle_r = data[0];
            }
            if (layer_name.find("angle_p") != string::npos) {
                head_angle_p = data[0];
            }
            if (layer_name.find("angle_y") != string::npos) {
                head_angle_y = data[0];
            }
        }
        if (!label.empty()) {
            auto pos = cv::Point2f(meta->x, meta->y + meta->h + 30);
            cv::putText(mat, label, pos, cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 255), 2);
        }
        if (head_angle_r != 0 && head_angle_p != 0 && head_angle_y != 0) {
            cv::Point3f center(meta->x + meta->w / 2, meta->y + meta->h / 2, 0);
            drawAxes(mat, center, head_angle_r, head_angle_p, head_angle_y, 50);
        }
    }

    gst_buffer_unmap(buffer, &map);
    gst_caps_unref(caps);
    GST_PAD_PROBE_INFO_DATA(info) = buffer;

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
    std::map<std::string, std::string> model_paths;
    std::string classify_str = "";
    if (detection_model == NULL) {
        for (const auto &model_to_path :
             FindModels(SplitString(env_models_path), default_detection_model_names, model_precision))
            model_paths.emplace(model_to_path);
        detection_model = g_strdup(model_paths["face-detection-adas-0001.xml"].c_str());
    }
    if (classification_models == NULL) {
        for (const auto &model_to_path :
             FindModels(SplitString(env_models_path), default_classification_model_names, model_precision))
            classify_str += "gvaclassify model=" + model_to_path.second + " device=" + device +
                            " batch-size=" + std::to_string(batch_size) + " ! queue ! ";
    }

    gchar const *preprocess_pipeline = "decodebin ! videoconvert n-threads=4 ! videoscale n-threads=4 ";
    gchar const *capfilter = "video/x-raw,format=BGRA";
    gchar const *sink = no_display ? "identity signal-handoffs=false ! fakesink sync=false"
                                   : "fpsdisplaysink video-sink=xvimagesink sync=false";

    // Build the pipeline
    auto launch_str = g_strdup_printf("filesrc location=%s ! %s ! capsfilter caps=\"%s\" ! "
                                      "gvadetect model=%s device=%s batch-size=%d ! queue ! "
                                      "%s"
                                      "gvawatermark name=gvawatermark ! videoconvert n-threads=4 ! %s",
                                      input_file, preprocess_pipeline, capfilter, detection_model, device, batch_size,
                                      classify_str.c_str(), sink);
    g_print("PIPELINE: %s \n", launch_str);
    GstElement *pipeline = gst_parse_launch(launch_str, NULL);
    g_free(launch_str);

    // set probe callback
    auto gvawatermark = gst_bin_get_by_name(GST_BIN(pipeline), "gvawatermark");
    auto pad = gst_element_get_static_pad(gvawatermark, "src");
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
