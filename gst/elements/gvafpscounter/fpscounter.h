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
    IterativeFpsCounter(unsigned interval, bool print_each_stream = true)
        : interval(interval), print_each_stream(print_each_stream) {
    }
    bool NewFrame(const std::string &element_name, FILE *output) override;
    void EOS(FILE *) override {
    }

  protected:
    unsigned interval;
    bool print_each_stream;
    std::chrono::time_point<std::chrono::high_resolution_clock> last_time;
    std::map<std::string, int> num_frames;
    std::mutex mutex;

    void PrintFPS(FILE *output, double sec);
};

class AverageFpsCounter : public FpsCounter {
  public:
    AverageFpsCounter(unsigned skipped_frames)
        : skipped_frames(skipped_frames), total_frames(0), result_reported(false) {
    }

    bool NewFrame(const std::string &element_name, FILE *) override;
    void EOS(FILE *output) override;

  protected:
    unsigned skipped_frames;
    unsigned total_frames;
    bool result_reported;
    std::chrono::time_point<std::chrono::high_resolution_clock> last_time;
    std::map<std::string, int> num_frames;
    std::mutex mutex;

    void PrintFPS(FILE *output, double sec);
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
