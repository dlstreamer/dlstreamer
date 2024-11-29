/*******************************************************************************
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "fpscounter.h"

#include <assert.h>
#include <chrono>
#include <exception>
#include <mutex>
#include <stdexcept>
#ifdef __linux__
#include <unistd.h>
#endif

namespace {
constexpr double TIME_THRESHOLD = 0.1;
using seconds_double = std::chrono::duration<double>;
constexpr int ELEMENT_NAME_MAX_SIZE = 64;
} // namespace

////////////////////////////////////////////////////////////////////////////////
// IterativeFpsCounter

bool IterativeFpsCounter::NewFrame(const std::string &element_name, FILE *output) {
    std::lock_guard<std::mutex> lock(mutex);
    if (++total_frames <= starting_frame)
        return false;
    if (output == nullptr)
        return false;

    auto now = std::chrono::high_resolution_clock::now();
    if (!init_time.time_since_epoch().count()) {
        init_time = last_time = now;
    }

    // reset average counter everytime a new stream is detected
    if (average && (num_frames.find(element_name) == num_frames.end())) {
        init_time = last_time = now;
        for (auto it = num_frames.begin(); it != num_frames.end(); it++)
            it->second = 0;
    }

    num_frames[element_name]++;

    double sec = std::chrono::duration_cast<seconds_double>(now - last_time).count();
    if (sec >= interval) {
        last_time = now;
        if (average) {
            sec = std::chrono::duration_cast<seconds_double>(now - init_time).count();
            PrintFPS(output, sec);
        } else {
            PrintFPS(output, sec);
            for (auto it = num_frames.begin(); it != num_frames.end(); it++)
                it->second = 0;
        }
        return true;
    }
    return false;
}

void IterativeFpsCounter::PrintFPS(FILE *output, double sec, bool eos) {
    assert(output);

    if (sec < TIME_THRESHOLD) {
        fprintf(output, "FPSCounter: Not enough data for calculation. The time interval (%.7f sec) is too short.\n",
                sec);
        return;
    }
    if (num_frames.empty())
        return;

    double total = 0;
    for (const auto &num : num_frames)
        total += num.second;
    total /= sec;

    if (average) {
        if (eos)
            fprintf(output, "FpsCounter(overall %.2fsec): ", sec);
        else
            fprintf(output, "FpsCounter(average %.2fsec): ", sec);
    } else {
        fprintf(output, "FpsCounter(last %.2fsec): ", sec);
    }
    fprintf(output, "total=%.2f fps, number-streams=%ld, per-stream=%.2f fps", total, num_frames.size(),
            total / num_frames.size());
    if (num_frames.size() > 1 && print_each_stream) {
        fprintf(output, " (");
        auto num = num_frames.begin();
        fprintf(output, "%.2f", num->second / sec);
        for (++num; num != num_frames.end(); ++num)
            fprintf(output, ", %.2f", num->second / sec);
        fprintf(output, ")");
    }
    fprintf(output, "\n");
    fflush(output);
}

void IterativeFpsCounter::EOS(const std::string &element_name, FILE *output) {
    assert(output);
    std::lock_guard<std::mutex> lock(mutex);
    if (!eos_result_reported) {
        auto now = std::chrono::high_resolution_clock::now();
        auto last = average ? init_time : last_time;
        double sec = std::chrono::duration_cast<seconds_double>(now - last).count();
        PrintFPS(output, sec, true);
        eos_result_reported = true;
    }

    // remove stream from counter list
    if (num_frames.find(element_name) != num_frames.end()) {
        num_frames.erase(element_name);
        // reset counter if there are still active streams
        if (num_frames.size() > 0) {
            init_time = last_time = std::chrono::high_resolution_clock::now();
            for (auto it = num_frames.begin(); it != num_frames.end(); it++)
                it->second = 0;
            eos_result_reported = false;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// WritePipeFpsCounter

static int getProcessId() {
#ifdef __linux__
    return getpid();
#elif _WIN32
    assert(!"getpid() is not implemented for Windows.");
    return -1;
#endif
}

WritePipeFpsCounter::WritePipeFpsCounter(const char *pipe_name)
    : pipe(new NamedPipe(std::string(pipe_name), NamedPipe::Mode::WriteOnly)), pid(std::to_string(getProcessId())) {
}

bool WritePipeFpsCounter::NewFrame(const std::string &element_name, FILE *) {
    std::string name = element_name + "_" + pid;
    assert(name.size() < ELEMENT_NAME_MAX_SIZE && "WritePipe message length exceeded");
    name.resize(ELEMENT_NAME_MAX_SIZE);

    auto count_bytes = pipe->write(name.c_str(), ELEMENT_NAME_MAX_SIZE);

    if (count_bytes == -1)
        throw std::runtime_error("Error writing to FIFO file " + name);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// ReadPipeFpsCounter

ReadPipeFpsCounter::ReadPipeFpsCounter(const char *pipe_name, std::function<void(const char *)> new_message,
                                       std::function<void(void)> pipe_completed)
    : pipe(new NamedPipe(std::string(pipe_name), NamedPipe::Mode::ReadOnly)), new_message_callback(new_message),
      pipe_completion_callback(pipe_completed) {
    thread = std::thread([&] {
        try {
            char name[ELEMENT_NAME_MAX_SIZE];
            while (true) {
                int nbytes = pipe->read(name, ELEMENT_NAME_MAX_SIZE);

                // If there's not enough data - wait for 10 ms
                if (nbytes < ELEMENT_NAME_MAX_SIZE) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    // Since we're waiting and have nothing to do,
                    // let's find out how many pipe descriptors are open.
                    // If there're zero, our work here is done.
                    if (getOpenedByProcessesDescriptorsCount(pipe->getName(), "w") == 0)
                        break;
                    continue;
                }
                new_message_callback(name);
            }
            pipe_completion_callback();
        } catch (const std::exception &e) {
            printf("ReadPipe error: %s", e.what());
        }
    });
}

ReadPipeFpsCounter::~ReadPipeFpsCounter() {
    try {
        if (thread.joinable()) {
            thread.join();
        }
    } catch (const std::exception &e) {
        printf("An error occurred while destructing ReadPipe: %s", e.what());
    }
}
