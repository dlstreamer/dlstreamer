/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "speedometer.h"
#include "config.h"
#include "gva_buffer_map.h"
#include "gva_utils.h"
#include "inference_backend/logger.h"
#include "video_frame.h"

#include <assert.h>
#include <chrono>
#include <exception>
#include <map>
#include <math.h>
#include <memory>
#include <mutex>

#define UNUSED(x) (void)(x)

class Speedometer {
  public:
    using seconds_double = std::chrono::duration<double>;
    virtual ~Speedometer() = default;
    virtual bool NewFrame(const std::string &element_name, FILE *output, GstBuffer *buf) = 0;
    virtual void EOS(FILE *output) = 0;
};

static std::map<std::string, std::shared_ptr<Speedometer>> speedometers;
static std::mutex channels_mutex;
//////////////////////////////////////////////////////////////////////////
// C interface

class IterativeSpeedometer : public Speedometer {
  private:
    std::map<int, std::pair<int, int>> prev_centers_bb;
    std::map<int, std::vector<double>> velocities;

  public:
    IterativeSpeedometer(double interval, bool print_each_stream = true)
        : interval(interval), print_each_stream(print_each_stream) {
    }
    double CalcAverageSpeed(int object_id) {
        double res = 0;
        auto velocity_vector = velocities[object_id];
        for (auto vel : velocity_vector) {
            res += vel;
        }
        res /= velocity_vector.size();
        return res;
    }
    void PrintAverageSpeed() {
        for (auto object : velocities) {
            auto object_id = object.first;
            auto avg_speed = CalcAverageSpeed(object_id);
            fprintf(stdout, "Average speed of id %d = %f \n", object_id, avg_speed);
        }
    }
    bool NewFrame(const std::string &element_name, FILE *output, GstBuffer *buf) override {
        UNUSED(element_name);
        UNUSED(output);
        GVA::VideoFrame frame(buf);

        for (auto &roi : frame.regions()) {
            int object_id = roi.meta()->id;
            int cur_x_center = roi.meta()->x + roi.meta()->w / 2;
            int cur_y_center = roi.meta()->y + roi.meta()->h / 2;
            gdouble velocity = 0;
            gdouble avg_speed = 0;
            if (prev_centers_bb.find(object_id) == prev_centers_bb.end()) {
                prev_centers_bb[object_id] = std::pair<int, int>(cur_x_center, cur_y_center);

            } else {
                auto now = std::chrono::high_resolution_clock::now();
                if (!last_time.time_since_epoch().count()) {
                    last_time = now;
                }

                gdouble sec = std::chrono::duration_cast<seconds_double>(now - last_time).count();

                if (sec >= interval) {
                    last_time = now;
                    auto prev_bb = prev_centers_bb[object_id];
                    gdouble d_bb = sqrt((cur_x_center - prev_bb.first) * (cur_x_center - prev_bb.first) +
                                        (cur_y_center - prev_bb.second) * (cur_y_center - prev_bb.second));
                    velocity = d_bb / interval;
                    velocities[object_id].push_back(velocity);
                    // PrintSpeed(stdout, object_id, velocity);

                    prev_centers_bb[object_id] = std::pair<int, int>(cur_x_center, cur_y_center);

                } else if (!velocities[object_id].empty()) {
                    velocity = velocities[object_id].back();
                    avg_speed = CalcAverageSpeed(object_id);
                } else {
                    avg_speed = 0;
                }

                auto result = gst_structure_new_empty("Velocity");
                gst_structure_set(result, "velocity", G_TYPE_DOUBLE, velocity, "id", G_TYPE_INT, object_id,
                                  "avg_velocity", G_TYPE_DOUBLE, avg_speed, NULL);
                GstVideoRegionOfInterestMeta *meta = roi.meta();
                gst_video_region_of_interest_meta_add_param(meta, result);
            }
        }
        return true;
    }
    void EOS(FILE *) override {
    }

  protected:
    double interval;
    bool print_each_stream;
    std::chrono::time_point<std::chrono::high_resolution_clock> last_time;
    std::map<std::string, int> num_frames;
    std::mutex mutex;

    void PrintSpeed(FILE *output, int id, double velocity) {

        fprintf(output, "Current speed of id %d = %f \n", id, velocity);
    }
};

extern "C" {

void create_iterative_speedometer(const char *intervals) {
    try {
        std::lock_guard<std::mutex> lock(channels_mutex);
        std::vector<std::string> intervals_list = SplitString(intervals, ',');
        for (const std::string &interval : intervals_list)
            if (not speedometers.count(interval)) {
                std::shared_ptr<Speedometer> speedometer =
                    std::shared_ptr<Speedometer>(new IterativeSpeedometer(std::stod(interval)));
                speedometers.insert({interval, speedometer});
            }
    } catch (std::exception &e) {
        std::string msg = std::string("Error during creation iterative speedometer: ") + e.what();
        GVA_ERROR(msg.c_str());
    }
}

void speedometer_new_frame(GstBuffer *buf, const char *element_name) {
    try {
        for (auto counter = speedometers.begin(); counter != speedometers.end(); ++counter)
            counter->second->NewFrame(element_name, stdout, buf);
    } catch (std::exception &e) {
        std::string msg = std::string("Error during adding new frame: ") + e.what();
        GVA_ERROR(msg.c_str());
    }
}

void speedometer_eos() {
    try {
        for (auto counter = speedometers.begin(); counter != speedometers.end(); ++counter)
            counter->second->EOS(stdout);
    } catch (std::exception &e) {
        std::string msg = std::string("Error during handling EOS : ") + e.what();
        GVA_ERROR(msg.c_str());
    }
}

} /* extern "C" */