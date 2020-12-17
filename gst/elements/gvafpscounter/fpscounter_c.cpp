/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "fpscounter_c.h"
#include "config.h"
#include "fpscounter.h"
#include "inference_backend/logger.h"
#include "utils.h"

#include <assert.h>
#include <exception>
#include <map>
#include <memory>
#include <mutex>

static std::map<std::string, std::shared_ptr<FpsCounter>> fps_counters;
static std::mutex channels_mutex;
static FILE *output = stdout;
//////////////////////////////////////////////////////////////////////////
// C interface
void fps_counter_create_iterative(const char *intervals) {
    try {
        std::lock_guard<std::mutex> lock(channels_mutex);
        std::vector<std::string> intervals_list = Utils::splitString(intervals, ',');
        for (const std::string &interval : intervals_list)
            if (not fps_counters.count(interval)) {
                std::shared_ptr<FpsCounter> fps_counter =
                    std::shared_ptr<FpsCounter>(new IterativeFpsCounter(std::stoi(interval)));
                fps_counters.insert({interval, fps_counter});
            }
    } catch (std::exception &e) {
        std::string msg = std::string("Error during creation iterative fpscounter: ") + e.what();
        GVA_ERROR(msg.c_str());
    }
}

void fps_counter_create_average(unsigned int starting_frame) {
    try {
        if (not fps_counters.count("average"))
            fps_counters.insert({"average", std::shared_ptr<FpsCounter>(new AverageFpsCounter(starting_frame))});
    } catch (std::exception &e) {
        std::string msg = std::string("Error during creation average fpscounter: ") + e.what();
        GVA_ERROR(msg.c_str());
    }
}

void fps_counter_new_frame(GstBuffer *, const char *element_name) {
    try {
        for (auto counter = fps_counters.begin(); counter != fps_counters.end(); ++counter)
            counter->second->NewFrame(element_name, output);
    } catch (std::exception &e) {
        std::string msg = std::string("Error during adding new frame: ") + e.what();
        GVA_ERROR(msg.c_str());
    }
}

void fps_counter_eos() {
    try {
        for (auto counter = fps_counters.begin(); counter != fps_counters.end(); ++counter)
            counter->second->EOS(output);
    } catch (std::exception &e) {
        std::string msg = std::string("Error during handling EOS : ") + e.what();
        GVA_ERROR(msg.c_str());
    }
}

void fps_counter_set_output(FILE *out) {
    if (out != nullptr)
        output = out;
}