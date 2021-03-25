/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "fpscounter.h"
#include "config.h"
#include "utils.h"

#include <assert.h>
#include <chrono>
#include <memory>
#include <mutex>

constexpr double TIME_THRESHOLD = 0.1;
using seconds_double = std::chrono::duration<double>;

bool IterativeFpsCounter::NewFrame(const std::string &element_name, FILE *output) {
    std::lock_guard<std::mutex> lock(mutex);
    if (output == nullptr)
        return false;
    num_frames[element_name]++;
    auto now = std::chrono::high_resolution_clock::now();
    if (!last_time.time_since_epoch().count()) {
        last_time = now;
    }

    double sec = std::chrono::duration_cast<seconds_double>(now - last_time).count();
    if (sec >= interval) {
        last_time = now;
        PrintFPS(output, sec);
        for (auto it = num_frames.begin(); it != num_frames.end(); it++)
            it->second = 0;
        return true;
    }
    return false;
}

void IterativeFpsCounter::PrintFPS(FILE *output, double sec) {
    if (!num_frames.size())
        return;
    double total = 0;
    for (const auto &num : num_frames)
        total += num.second;
    total /= sec;

    fprintf(output, "FpsCounter(%dsec): ", interval);
    fprintf(output, "total=%.2f fps, number-streams=%ld, per-stream=%.2f fps", total, num_frames.size(),
            total / num_frames.size());
    if (num_frames.size() > 1 and print_each_stream) {
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

bool AverageFpsCounter::NewFrame(const std::string &element_name, FILE *) {
    std::lock_guard<std::mutex> lock(mutex);
    total_frames++;
    if (total_frames <= skipped_frames)
        return false;
    num_frames[element_name]++;
    if (!last_time.time_since_epoch().count()) {
        last_time = std::chrono::high_resolution_clock::now();
    }
    return true;
}

void AverageFpsCounter::EOS(FILE *output) {
    assert(output);
    std::lock_guard<std::mutex> lock(mutex);
    if (not result_reported) {
        auto now = std::chrono::high_resolution_clock::now();
        double sec = std::chrono::duration_cast<seconds_double>(now - last_time).count();
        PrintFPS(output, sec);
        result_reported = true;
    }
}

void AverageFpsCounter::PrintFPS(FILE *output, double sec) {
    if (sec < TIME_THRESHOLD) {
        fprintf(output,
                "FPSCounter(average): Not enough data for calculation. The time interval (%.7f sec) is too short.\n",
                sec);
        return;
    }

    if (!num_frames.size())
        return;
    double total = 0;
    for (auto num : num_frames)
        total += num.second;
    total /= sec;

    fprintf(output, "FPSCounter(average): ");
    fprintf(output, "total=%.2f fps, number-streams=%ld, per-stream=%.2f fps", total, num_frames.size(),
            total / num_frames.size());
    if (num_frames.size() > 1) {
        fprintf(output, " (");
        for (auto num = num_frames.begin(); num != num_frames.end(); num++) {
            if (num != num_frames.begin())
                fprintf(output, ", ");
            fprintf(output, "%.2f", num->second / sec);
        }
        fprintf(output, ")");
    }
    fprintf(output, "\n");
}
