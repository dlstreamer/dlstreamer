/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "named_pipe.h"

#include <chrono>
#include <functional>
#include <gst/video/video.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

class FpsCounter {
  public:
    virtual ~FpsCounter() = default;
    virtual bool NewFrame(const std::string &element_name, FILE *output) = 0;
    virtual void EOS(FILE *output) = 0;
};

class IterativeFpsCounter : public FpsCounter {
  public:
    IterativeFpsCounter(unsigned starting_frame, unsigned interval, bool average)
        : starting_frame(starting_frame), interval(interval), average(average), print_each_stream(true),
          total_frames(0), eos_result_reported(false) {
    }
    bool NewFrame(const std::string &element_name, FILE *output) override;
    void EOS(FILE *) override;

  protected:
    unsigned starting_frame;
    unsigned interval;
    bool average;
    bool print_each_stream;
    unsigned total_frames;
    std::chrono::time_point<std::chrono::high_resolution_clock> init_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> last_time;
    std::map<std::string, int> num_frames;
    std::mutex mutex;
    bool eos_result_reported;

    void PrintFPS(FILE *output, double sec, bool eos = false);
};

class WritePipeFpsCounter : public FpsCounter {
  public:
    WritePipeFpsCounter(const char *pipe_name);
    bool NewFrame(const std::string &element_name, FILE *output) override;
    void EOS(FILE *) override {
    }

  protected:
    std::unique_ptr<NamedPipe> pipe;
    std::string pid;
};

class ReadPipeFpsCounter : public FpsCounter {
  public:
    ReadPipeFpsCounter(const char *pipe_name, std::function<void(const char *)> new_message,
                       std::function<void(void)> pipe_completed);
    ~ReadPipeFpsCounter();
    bool NewFrame(const std::string &, FILE *) override {
        return true;
    }
    void EOS(FILE *) override {
    }

  protected:
    std::unique_ptr<NamedPipe> pipe;
    std::thread thread;
    std::function<void(const char *)> new_message_callback;
    std::function<void(void)> pipe_completion_callback;
};
