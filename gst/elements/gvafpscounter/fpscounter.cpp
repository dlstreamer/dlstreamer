/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "fpscounter.h"
#include "config.h"
#include "gva_utils.h"
#include "inference_backend/logger.h"

#include <assert.h>
#include <chrono>
#include <exception>
#include <map>
#include <memory>
#include <mutex>

class FpsCounter {
  public:
    using seconds_double = std::chrono::duration<double>;
    virtual ~FpsCounter() = default;
    virtual bool NewFrame(const std::string &element_name, FILE *output) = 0;
    virtual void EOS(FILE *output) = 0;
};

static std::map<std::string, std::shared_ptr<FpsCounter>> fps_counters;
static std::mutex channels_mutex;
//////////////////////////////////////////////////////////////////////////
// C interface

class IterativeFpsCounter : public FpsCounter {
  public:
    IterativeFpsCounter(unsigned interval, bool print_each_stream = true)
        : interval(interval), print_each_stream(print_each_stream) {
    }
    bool NewFrame(const std::string &element_name, FILE *output) override {
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
            num_frames.clear();
            return true;
        }
        return false;
    }
    void EOS(FILE *) override {
    }

  protected:
    unsigned interval;
    bool print_each_stream;
    std::chrono::time_point<std::chrono::high_resolution_clock> last_time;
    std::map<std::string, int> num_frames;
    std::mutex mutex;

    void PrintFPS(FILE *output, double sec) {
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
    }
};

class AverageFpsCounter : public FpsCounter {
  public:
    AverageFpsCounter(unsigned skipped_frames)
        : skipped_frames(skipped_frames), total_frames(0), result_reported(false) {
    }

    bool NewFrame(const std::string &element_name, FILE *) override {
        std::lock_guard<std::mutex> lock(mutex);
        total_frames++;
        if (total_frames < skipped_frames)
            return false;
        num_frames[element_name]++;
        if (!last_time.time_since_epoch().count()) {
            last_time = std::chrono::high_resolution_clock::now();
        }
        return true;
    }
    void EOS(FILE *output) override {
        assert(output);
        std::lock_guard<std::mutex> lock(mutex);
        if (not result_reported) {
            auto now = std::chrono::high_resolution_clock::now();
            double sec = std::chrono::duration_cast<seconds_double>(now - last_time).count();
            PrintFPS(output, sec);
            result_reported = true;
        }
    }

  protected:
    unsigned skipped_frames;
    unsigned total_frames;
    bool result_reported;
    std::chrono::time_point<std::chrono::high_resolution_clock> last_time;
    std::map<std::string, int> num_frames;
    std::mutex mutex;

    void PrintFPS(FILE *output, double sec) {
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
};

extern "C" {

void create_iterative_fps_counter(const char *intervals) {
    try {
        std::lock_guard<std::mutex> lock(channels_mutex);
        std::vector<std::string> intervals_list = SplitString(intervals, ',');
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

void create_average_fps_counter(unsigned int starting_frame) {
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
            counter->second->NewFrame(element_name, stdout);
    } catch (std::exception &e) {
        std::string msg = std::string("Error during adding new frame: ") + e.what();
        GVA_ERROR(msg.c_str());
    }
}

void fps_counter_eos() {
    try {
        for (auto counter = fps_counters.begin(); counter != fps_counters.end(); ++counter)
            counter->second->EOS(stdout);
    } catch (std::exception &e) {
        std::string msg = std::string("Error during handling EOS : ") + e.what();
        GVA_ERROR(msg.c_str());
    }
}

} /* extern "C" */
