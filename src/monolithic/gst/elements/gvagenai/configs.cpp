/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "configs.hpp"

#include <gst/gst.h>
GST_DEBUG_CATEGORY_EXTERN(gst_gvagenai_debug);
#define GST_CAT_DEFAULT gst_gvagenai_debug

namespace genai {

std::string ConfigParser::trim(const std::string &str) {
    size_t first = str.find_first_not_of(" \t");
    if (first == std::string::npos)
        return "";
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, (last - first + 1));
}

template <typename T>
void ConfigParser::convert_prop(ov::AnyMap &properties, const std::string &key, const std::string &value,
                                const std::string &ov_prop_name, ov::Property<T, ov::PropertyMutability::RW> ov_prop) {
    if (ov_prop_name != key)
        return;
    std::stringstream ss(value);
    T ov_val;
    if constexpr (std::is_same_v<T, bool>) {
        ss >> std::boolalpha >> ov_val;
    } else {
        ss >> ov_val;
    }
    if (ss.fail()) {
        GST_ERROR("Cannot convert %s to expected type for property %s", value.c_str(), key.c_str());
        return;
    }
    properties.emplace(ov_prop(ov_val));
    GST_INFO("Set generation config: %s = %s", key.c_str(), value.c_str());
}

template <typename T>
void ConfigParser::convert_prop(ov::AnyMap &properties, const std::string &key, const std::string &value,
                                const std::string &ov_prop_name,
                                ov::Property<std::set<T>, ov::PropertyMutability::RW> ov_prop) {
    if (ov_prop_name != key)
        return;
    std::set<T> items_set;
    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, ';')) { // Use semicolon as separator
        item = trim(item);
        if (!item.empty()) {
            if constexpr (std::is_same_v<T, std::string>) {
                items_set.insert(item);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                try {
                    int64_t token_id = std::stoll(item);
                    items_set.insert(token_id);
                } catch (const std::exception &e) {
                    GST_ERROR("Invalid token ID: %s", item.c_str());
                }
            } else {
                GST_ERROR("Unsupported type for set property: %s", typeid(T).name());
                return;
            }
        }
    }
    properties.emplace(ov_prop(items_set));
    GST_INFO("Set generation config: %s with %zu items", ov_prop_name.c_str(), items_set.size());
}

void ConfigParser::convert_prop(ov::AnyMap &properties, const std::string &key, const std::string &value,
                                const std::string &ov_prop_name,
                                ov::Property<ov::genai::StopCriteria, ov::PropertyMutability::RW> ov_prop) {
    if (ov_prop_name != key)
        return;
    ov::genai::StopCriteria criteria;
    if (value == "EARLY") {
        criteria = ov::genai::StopCriteria::EARLY;
    } else if (value == "HEURISTIC") {
        criteria = ov::genai::StopCriteria::HEURISTIC;
    } else if (value == "NEVER") {
        criteria = ov::genai::StopCriteria::NEVER;
    } else {
        GST_WARNING("Invalid stop_criteria value: %s. Valid values are: EARLY, "
                    "HEURISTIC, NEVER",
                    value.c_str());
        return;
    }
    properties.emplace(ov_prop(criteria));
    GST_INFO("Set generation config: %s = %s", ov_prop_name.c_str(), value.c_str());
}

ov::AnyMap ConfigParser::convert_to_properties(const std::map<std::string, std::string> &config_map) {
    ov::AnyMap properties;

    for (const auto &pair : config_map) {
        const std::string &k = pair.first;
        const std::string &v = pair.second;

        // Generic parameters
        convert_prop(properties, k, v, "max_new_tokens", ov::genai::max_new_tokens);
        convert_prop(properties, k, v, "max_length", ov::genai::max_length);
        convert_prop(properties, k, v, "ignore_eos", ov::genai::ignore_eos);
        convert_prop(properties, k, v, "min_new_tokens", ov::genai::min_new_tokens);

        // EOS and stop parameters
        convert_prop(properties, k, v, "eos_token_id", ov::genai::eos_token_id);
        convert_prop(properties, k, v, "stop_strings", ov::genai::stop_strings);
        convert_prop(properties, k, v, "include_stop_str_in_output", ov::genai::include_stop_str_in_output);
        convert_prop(properties, k, v, "stop_token_ids", ov::genai::stop_token_ids);

        // Penalties
        convert_prop(properties, k, v, "repetition_penalty", ov::genai::repetition_penalty);
        convert_prop(properties, k, v, "presence_penalty", ov::genai::presence_penalty);
        convert_prop(properties, k, v, "frequency_penalty", ov::genai::frequency_penalty);

        // Beam search specific parameters
        convert_prop(properties, k, v, "num_beams", ov::genai::num_beams);
        convert_prop(properties, k, v, "num_beam_groups", ov::genai::num_beam_groups);
        convert_prop(properties, k, v, "diversity_penalty", ov::genai::diversity_penalty);
        convert_prop(properties, k, v, "length_penalty", ov::genai::length_penalty);
        convert_prop(properties, k, v, "num_return_sequences", ov::genai::num_return_sequences);
        convert_prop(properties, k, v, "no_repeat_ngram_size", ov::genai::no_repeat_ngram_size);
        convert_prop(properties, k, v, "stop_criteria", ov::genai::stop_criteria);

        // Random sampling parameters
        convert_prop(properties, k, v, "do_sample", ov::genai::do_sample);
        convert_prop(properties, k, v, "temperature", ov::genai::temperature);
        convert_prop(properties, k, v, "top_p", ov::genai::top_p);
        convert_prop(properties, k, v, "top_k", ov::genai::top_k);
        convert_prop(properties, k, v, "rng_seed", ov::genai::rng_seed);

        // Assisting generation parameters
        convert_prop(properties, k, v, "assistant_confidence_threshold", ov::genai::assistant_confidence_threshold);
        convert_prop(properties, k, v, "num_assistant_tokens", ov::genai::num_assistant_tokens);
        convert_prop(properties, k, v, "max_ngram_size", ov::genai::max_ngram_size);

        // Other parameters
        convert_prop(properties, k, v, "apply_chat_template", ov::genai::apply_chat_template);
    }

    return properties;
}

ov::AnyMap ConfigParser::parse_generation_config_string(const std::string &config_str) {
    // Return empty map if no configuration provided
    if (config_str.empty()) {
        return ov::AnyMap();
    }

    // Parse KEY=VALUE,KEY=VALUE format
    std::map<std::string, std::string> config_map;

    std::istringstream ss(config_str);
    std::string pair;
    while (std::getline(ss, pair, ',')) {
        pair = trim(pair);

        size_t pos = pair.find('=');
        if (pos != std::string::npos) {
            std::string key = trim(pair.substr(0, pos));
            std::string value = trim(pair.substr(pos + 1));

            if (!key.empty()) {
                config_map[key] = value;
            }
        }
    }
    return ConfigParser::convert_to_properties(config_map);
}

std::optional<ov::genai::SchedulerConfig> ConfigParser::parse_scheduler_config_string(const std::string &config_str) {
    // Return nullopt if no configuration provided
    if (config_str.empty()) {
        return std::nullopt;
    }

    // Parse KEY=VALUE,KEY=VALUE format and create scheduler config
    ov::genai::SchedulerConfig scheduler_config;

    // Collect cache eviction parameters
    std::map<std::string, std::string> cache_eviction_params;

    std::istringstream ss(config_str);
    std::string pair;
    while (std::getline(ss, pair, ',')) {
        pair = trim(pair);

        size_t pos = pair.find('=');
        if (pos != std::string::npos) {
            std::string key = trim(pair.substr(0, pos));
            std::string value = trim(pair.substr(pos + 1));

            if (key.empty())
                continue;

            try {
                if (key == "max_num_batched_tokens") {
                    scheduler_config.max_num_batched_tokens = std::stoull(value);
                    GST_INFO("Set scheduler config: %s = %zu", key.c_str(), scheduler_config.max_num_batched_tokens);
                } else if (key == "num_kv_blocks") {
                    scheduler_config.num_kv_blocks = std::stoull(value);
                    GST_INFO("Set scheduler config: %s = %zu", key.c_str(), scheduler_config.num_kv_blocks);
                } else if (key == "cache_size") {
                    scheduler_config.cache_size = std::stoull(value);
                    GST_INFO("Set scheduler config: %s = %zu", key.c_str(), scheduler_config.cache_size);
                } else if (key == "dynamic_split_fuse") {
                    std::istringstream(value) >> std::boolalpha >> scheduler_config.dynamic_split_fuse;
                    GST_INFO("Set scheduler config: %s = %s", key.c_str(),
                             scheduler_config.dynamic_split_fuse ? "true" : "false");
                } else if (key == "use_cache_eviction") {
                    std::istringstream(value) >> std::boolalpha >> scheduler_config.use_cache_eviction;
                    GST_INFO("Set scheduler config: %s = %s", key.c_str(),
                             scheduler_config.use_cache_eviction ? "true" : "false");
                } else if (key == "max_num_seqs") {
                    scheduler_config.max_num_seqs = std::stoull(value);
                    GST_INFO("Set scheduler config: %s = %zu", key.c_str(), scheduler_config.max_num_seqs);
                } else if (key == "enable_prefix_caching") {
                    std::istringstream(value) >> std::boolalpha >> scheduler_config.enable_prefix_caching;
                    GST_INFO("Set scheduler config: %s = %s", key.c_str(),
                             scheduler_config.enable_prefix_caching ? "true" : "false");
                } else if (key.starts_with("cache_eviction_")) {
                    // Collect cache eviction config parameters
                    cache_eviction_params[key] = value;
                } else {
                    GST_WARNING("Unknown scheduler config key: %s", key.c_str());
                }
            } catch (const std::exception &e) {
                GST_ERROR("Failed to parse scheduler config value for key %s: %s. Error: %s", key.c_str(),
                          value.c_str(), e.what());
            }
        }
    }

    // Apply cache eviction config if any parameters were provided
    if (!cache_eviction_params.empty()) {
        try {
            // Get current values or use defaults
            size_t start_size = scheduler_config.cache_eviction_config.get_start_size();
            size_t recent_size = scheduler_config.cache_eviction_config.get_recent_size();
            size_t max_cache_size = scheduler_config.cache_eviction_config.get_max_cache_size();
            ov::genai::AggregationMode aggregation_mode = scheduler_config.cache_eviction_config.aggregation_mode;
            bool apply_rotation = scheduler_config.cache_eviction_config.apply_rotation;
            size_t snapkv_window_size = scheduler_config.cache_eviction_config.snapkv_window_size;

            // Apply collected parameters
            for (const auto &param : cache_eviction_params) {
                const std::string &key = param.first;
                const std::string &value = param.second;

                if (key == "cache_eviction_start_size") {
                    start_size = std::stoull(value);
                } else if (key == "cache_eviction_recent_size") {
                    recent_size = std::stoull(value);
                } else if (key == "cache_eviction_max_cache_size") {
                    max_cache_size = std::stoull(value);
                } else if (key == "cache_eviction_aggregation_mode") {
                    if (value == "SUM") {
                        aggregation_mode = ov::genai::AggregationMode::SUM;
                    } else if (value == "NORM_SUM") {
                        aggregation_mode = ov::genai::AggregationMode::NORM_SUM;
                    } else {
                        GST_WARNING("Invalid cache_eviction_aggregation_mode value: %s. "
                                    "Valid values are: SUM, NORM_SUM",
                                    value.c_str());
                        continue;
                    }
                } else if (key == "cache_eviction_apply_rotation") {
                    std::istringstream(value) >> std::boolalpha >> apply_rotation;
                } else if (key == "cache_eviction_snapkv_window_size") {
                    snapkv_window_size = std::stoull(value);
                }
            }

            // Create new cache eviction config
            scheduler_config.cache_eviction_config = ov::genai::CacheEvictionConfig(
                start_size, recent_size, max_cache_size, aggregation_mode, apply_rotation, snapkv_window_size);

            GST_INFO("Applied cache eviction config: start_size=%zu, "
                     "recent_size=%zu, max_cache_size=%zu, aggregation_mode=%d, "
                     "apply_rotation=%s, snapkv_window_size=%zu",
                     start_size, recent_size, max_cache_size, static_cast<int>(aggregation_mode),
                     apply_rotation ? "true" : "false", snapkv_window_size);
        } catch (const std::exception &e) {
            GST_ERROR("Failed to apply cache eviction config: %s", e.what());
        }
    }

    return scheduler_config;
}

} // namespace genai
