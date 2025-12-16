/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "genai.hpp"

GST_DEBUG_CATEGORY_EXTERN(gst_gvagenai_debug);
#define GST_CAT_DEFAULT gst_gvagenai_debug

#include "configs.hpp"

#include <nlohmann/json.hpp>

namespace genai {

OpenVINOGenAIContext::OpenVINOGenAIContext(const std::string &model_path, const std::string &device,
                                           const std::string &cache_path, const std::string &generation_config_str,
                                           const std::string &scheduler_config_str) {
    // Set configurations if provided
    if (!generation_config_str.empty()) {
        generation_config = ConfigParser::parse_generation_config_string(generation_config_str);
    }

    if (!scheduler_config_str.empty()) {
        scheduler_config = ConfigParser::parse_scheduler_config_string(scheduler_config_str);
    }

    ov::AnyMap properties;

    // Cache compiled models on disk for GPU to save time on the
    // next run. It's not beneficial for CPU.
    // TODO check NPU support for cache
    if (device.starts_with("GPU")) {
        properties.insert({ov::cache_dir(cache_path)});
    }

    if (get_scheduler_config()) {
        properties.insert({ov::genai::scheduler_config(*get_scheduler_config())});
    }

    GST_INFO("%s: %s", ov::get_openvino_version().description, ov::get_openvino_version().buildNumber);
    GST_INFO("Initializing OpenVINO™ GenAI VLM pipeline with model: %s on device: %s", model_path.c_str(),
             device.c_str());

    try {
        pipeline = std::make_unique<ov::genai::VLMPipeline>(model_path, device, properties);
        metrics = ov::genai::VLMPerfMetrics();
        GST_INFO("OpenVINO™ GenAI VLM pipeline initialized successfully");
    } catch (const std::exception &e) {
        GST_ERROR("Failed to initialize OpenVINO™ GenAI VLM pipeline: %s", e.what());
        throw std::runtime_error("Failed to initialize OpenVINO™ GenAI VLM pipeline");
    }
}

OpenVINOGenAIContext::~OpenVINOGenAIContext() {
    tensor_vector.clear();
}

bool OpenVINOGenAIContext::add_tensor_to_vector(GstBuffer *buffer, GstVideoInfo *info) {
    try {
        GstMapInfo map;
        if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            GST_ERROR("Failed to map buffer");
            return false;
        }

        // Convert GStreamer buffer to OpenCV Mat
        cv::Mat frame;
        switch (GST_VIDEO_INFO_FORMAT(info)) {
        case GST_VIDEO_FORMAT_RGB:
            frame = cv::Mat(info->height, info->width, CV_8UC3, map.data);
            break;
        case GST_VIDEO_FORMAT_RGBA:
        case GST_VIDEO_FORMAT_RGBx:
            frame = cv::Mat(info->height, info->width, CV_8UC4, map.data);
            cv::cvtColor(frame, frame, cv::COLOR_RGBA2RGB);
            break;
        case GST_VIDEO_FORMAT_BGR:
            frame = cv::Mat(info->height, info->width, CV_8UC3, map.data);
            cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
            break;
        case GST_VIDEO_FORMAT_BGRA:
        case GST_VIDEO_FORMAT_BGRx:
            frame = cv::Mat(info->height, info->width, CV_8UC4, map.data);
            cv::cvtColor(frame, frame, cv::COLOR_BGRA2RGB);
            break;
        case GST_VIDEO_FORMAT_NV12: {
            frame = cv::Mat(info->height * 3 / 2, info->width, CV_8UC1, map.data);
            cv::cvtColor(frame, frame, cv::COLOR_YUV2RGB_NV12);
            break;
        }
        case GST_VIDEO_FORMAT_I420: {
            frame = cv::Mat(info->height * 3 / 2, info->width, CV_8UC1, map.data);
            cv::cvtColor(frame, frame, cv::COLOR_YUV2RGB_I420);
            break;
        }
        default:
            gst_buffer_unmap(buffer, &map);
            GST_ERROR("Unsupported video format");
            return false;
        }

        // Create tensor
        auto tensor = ov::Tensor(ov::element::u8, {1, static_cast<unsigned long long>(frame.rows),
                                                   static_cast<unsigned long long>(frame.cols),
                                                   static_cast<unsigned long long>(frame.channels())});
        memcpy(tensor.data(), frame.data, frame.total() * frame.elemSize());

        // Add tensor to vector
        tensor_vector.push_back(tensor);

        gst_buffer_unmap(buffer, &map);
        return true;
    } catch (const std::exception &e) {
        GST_ERROR("Error converting frame to tensor: %s", e.what());
        return false;
    }
}

bool OpenVINOGenAIContext::inference_tensor_vector(const std::string &prompt) {
    if (tensor_vector.empty()) {
        GST_ERROR("Tensor vector is empty");
        return false;
    }

    try {
        ov::AnyMap properties = generation_config;

        // Set default max_new_tokens=100 if not specified
        if (properties.find(ov::genai::max_new_tokens.name()) == properties.end()) {
            properties.emplace(ov::genai::max_new_tokens(100));
        }

        // Add images to properties
        properties.emplace(ov::genai::images(tensor_vector));

        // Run inference, this is a long blocking call
        GST_INFO("Running inference with %ld images and prompt: %s", tensor_vector.size(), prompt.c_str());
        auto result = pipeline->generate(prompt, properties);
        GST_INFO("Inference completed successfully");

        // Store results and metrics
        last_result.clear();
        for (const auto &text : result.texts) {
            last_result += text;
        }

        // Update metrics
        if (metrics.load_time == 0) {
            metrics = result.perf_metrics;
        } else {
            metrics += result.perf_metrics;
        }

        // Clear the tensor vector
        tensor_vector.clear();
        return true;
    } catch (const std::exception &e) {
        GST_ERROR("Error during inference: %s", e.what());
        return false;
    }
}

size_t OpenVINOGenAIContext::get_tensor_vector_size() const {
    return tensor_vector.size();
}

void OpenVINOGenAIContext::clear_tensor_vector() {
    tensor_vector.clear();
}

void OpenVINOGenAIContext::set_generation_config(const std::string &config_str) {
    generation_config = ConfigParser::parse_generation_config_string(config_str);
}

void OpenVINOGenAIContext::set_scheduler_config(const std::string &config_str) {
    scheduler_config = ConfigParser::parse_scheduler_config_string(config_str);
}

ov::AnyMap OpenVINOGenAIContext::get_generation_config() const {
    return generation_config;
}

std::optional<ov::genai::SchedulerConfig> OpenVINOGenAIContext::get_scheduler_config() const {
    return scheduler_config;
}

std::string OpenVINOGenAIContext::get_last_result() const {
    return last_result;
}

std::string OpenVINOGenAIContext::create_json_metadata(GstClockTime timestamp, bool include_metrics) {
    auto round_2dp = [](double value) { return std::round(value * 100.0) / 100.0; };

    nlohmann::json json_obj = {{"result", last_result}};
    if (include_metrics) {
        nlohmann::json metrics_obj = {
            {"load_time", round_2dp(metrics.get_load_time())},
            {"generate_time_mean", round_2dp(metrics.get_generate_duration().mean)},
            {"generate_time_std", round_2dp(metrics.get_generate_duration().std)},
            {"tokenization_time_mean", round_2dp(metrics.get_tokenization_duration().mean)},
            {"tokenization_time_std", round_2dp(metrics.get_tokenization_duration().std)},
            {"detokenization_time_mean", round_2dp(metrics.get_detokenization_duration().mean)},
            {"detokenization_time_std", round_2dp(metrics.get_detokenization_duration().std)},
            {"embeddings_prep_time_mean", round_2dp(metrics.get_prepare_embeddings_duration().mean)},
            {"embeddings_prep_time_std", round_2dp(metrics.get_prepare_embeddings_duration().std)},
            {"ttft_mean", round_2dp(metrics.get_ttft().mean)},
            {"ttft_std", round_2dp(metrics.get_ttft().std)},
            {"tpot_mean", round_2dp(metrics.get_tpot().mean)},
            {"tpot_std", round_2dp(metrics.get_tpot().std)},
            {"throughput_mean", round_2dp(metrics.get_throughput().mean)},
            {"throughput_std", round_2dp(metrics.get_throughput().std)}};
        json_obj["metrics"] = metrics_obj;
    }
    if (GST_CLOCK_TIME_IS_VALID(timestamp)) {
        json_obj["timestamp"] = timestamp;
        json_obj["timestamp_seconds"] = round_2dp((double)timestamp / GST_SECOND);
    }
    return json_obj.dump();
}

} // namespace genai
