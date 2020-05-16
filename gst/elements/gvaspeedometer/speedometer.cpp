/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "speedometer.h"
#include "config.h"
#include "gva_utils.h"
#include "gva_buffer_map.h"
#include "inference_backend/logger.h"

#include <assert.h>
#include <chrono>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <math.h>
#include <video_frame.h>

#define UNUSED(x) (void)(x)
#define BB_SIZE 4

//TODO: make config

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
        std::map<int, std::array<unsigned int, BB_SIZE >> prev_bb;
        std::map<int, std::vector<double>> velocities;
        std::map<int, int> violations_num;
        std::map<int, bool> violations;
  public:
    IterativeSpeedometer(double interval, double alpha, double alpha_hw, 
            double speedlimit, int speedlimit_violations, bool print_each_stream = true)
        : interval(interval), alpha(alpha), alpha_hw(alpha_hw), speedlimit(speedlimit), speedlimit_violations(speedlimit_violations),
         print_each_stream(print_each_stream) {
            
    }
    double CalcAverageSpeed(int object_id)
    {
        double res = 0;
        auto velocity_vector = velocities[object_id];
        for (double vel : velocity_vector){
            
            res += 1.0 / vel;
        }
        
        res /= velocity_vector.size();
        return 1.0 / res;
    }
    void PrintAverageSpeed() {
        for (auto object : velocities){
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

            
            gdouble velocity = 0;
            gdouble avg_speed = 0;
            if ( prev_centers_bb.find(object_id) == prev_centers_bb.end() ) {
                // first detection
                int cur_x_center = roi.meta()->x + roi.meta()->w / 2;
                int cur_y_center = roi.meta()->y + roi.meta()->h / 2;
                prev_centers_bb[object_id] = std::pair<int, int> (cur_x_center, cur_y_center);
                std::array<unsigned int, BB_SIZE> prev_bb_coords = {roi.meta()->x, roi.meta()->y, roi.meta()->h, roi.meta()->w};
                prev_bb[object_id] = prev_bb_coords;
                violations_num[object_id] = 0;
                violations[object_id] = false;
                
            }
            else {
                // second and other detections
                auto now = std::chrono::high_resolution_clock::now();
                if (!last_time.time_since_epoch().count()) {
                    last_time = now;
                }
                auto prev_bb_coords = prev_bb[object_id];

                auto new_x = static_cast<unsigned int> (static_cast<double> (prev_bb_coords[0]) + alpha * (
                             static_cast<double> (roi.meta()->x) - static_cast<double> (prev_bb_coords[0]) )  );
                auto new_y = static_cast<unsigned int> (static_cast<double> (prev_bb_coords[1]) + alpha * (
                             static_cast<double> (roi.meta()->y) - static_cast<double> (prev_bb_coords[1]) )  );
                auto new_h = static_cast<unsigned int> (static_cast<double> (prev_bb_coords[2]) + alpha_hw * (
                             static_cast<double> (roi.meta()->h) - static_cast<double> (prev_bb_coords[2]) )  );
                auto new_w = static_cast<unsigned int> (static_cast<double> (prev_bb_coords[3]) + alpha_hw * (
                             static_cast<double> (roi.meta()->w) - static_cast<double> (prev_bb_coords[3]) )  );
                roi.meta()->x = new_x;
                roi.meta()->y = new_y;
                roi.meta()->h = new_h;
                roi.meta()->w = new_w;
                prev_bb[object_id] = {new_x, new_y, new_h, new_w};
                gdouble sec = std::chrono::duration_cast<seconds_double>(now - last_time).count();

                // if (sec >= interval) {
                    
                //     last_time = now;

                auto prev_bb = prev_centers_bb[object_id];
                int cur_x_center = roi.meta()->x + roi.meta()->w / 2;
                int cur_y_center = roi.meta()->y + roi.meta()->h / 2;
                // auto cur_bb = std::pair<int, int> (cur_x_center, cur_y_center)
                // auto smothed_bb = std::pair<int, int> (prev_bb.first + ALPHA * (cur_x_center - prev_bb.first), 
                //         prev_bb.second + ALPHA * (cur_x_center - prev_bb.second) )
                gdouble d_bb = sqrt( (cur_x_center - prev_bb.first) * (cur_x_center - prev_bb.first) + 
                    (cur_y_center - prev_bb.second) * (cur_y_center - prev_bb.second) );
                // gdouble d_bb = sqrt( (smothed_bb.first - prev_bb.first) * (smothed_bb.first - prev_bb.first) + 
                //      (smothed_bb.second - prev_bb.second) * (smothed_bb.second - prev_bb.second) );
                velocity = d_bb; // / interval;
                velocities[object_id].push_back(velocity);
                fprintf(stdout, "%f\t", velocity);
                //PrintSpeed(stdout, object_id, velocity);
                prev_centers_bb[object_id] = std::pair<int, int> (cur_x_center, cur_y_center);
                avg_speed = CalcAverageSpeed(object_id);
                if (avg_speed >= speedlimit)
                {   
                    // fprintf(stdout, "Average speed of id %d = %f \n", object_id, avg_speed);
                    violations_num[object_id] += 1;
                }
                if (violations_num[object_id] >= speedlimit_violations)
                {
                    // if (!violations[object_id])
                    //     fprintf(stdout, "Warning! Id %d possibly violates speed limit \n", object_id);
                    violations[object_id] = true;
                }

                    
                // }
                // else 
                if ( ! velocities[object_id].empty() )
                {
                    velocity = velocities[object_id].back();
                    avg_speed = CalcAverageSpeed(object_id);
                    
                }
                else
                {
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
    double alpha;
    double alpha_hw;
    double speedlimit;
    int speedlimit_violations;
    bool print_each_stream;
    std::chrono::time_point<std::chrono::high_resolution_clock> last_time;
    std::map<std::string, int> num_frames;
    std::mutex mutex;

    void PrintSpeed(FILE *output, int id, double velocity) {

        
        fprintf(output, "Current speed of id %d = %f \n", id, velocity);

    }
};



extern "C" {

void create_iterative_speedometer (const char *intervals, const char *alphas, const char *alphas_hw, 
        const char *speedlimits, const char *speedlimit_violationss) {
    try {
        std::lock_guard<std::mutex> lock(channels_mutex);
        std::vector<std::string> intervals_list = SplitString(intervals, ',');
        std::vector<std::string> alphas_list = SplitString(alphas, ',');
        std::vector<std::string> alphas_hw_list = SplitString(alphas_hw, ',');
        std::vector<std::string> speedlimits_list = SplitString(speedlimits, ',');
        std::vector<std::string> speedlimit_violations_list = SplitString(speedlimit_violationss, ',');
        for (unsigned counter = 0; counter < intervals_list.size(); counter++ ) {
            //std::string &interval : intervals_list)
            auto interval = intervals_list[counter];
            auto alpha = alphas_list[counter];
            auto alpha_hw = alphas_hw_list[counter];
            auto speedlim = speedlimits_list[counter];
            auto speedlim_viols = speedlimit_violations_list[counter];
            if (not speedometers.count(interval)) {
                std::shared_ptr<Speedometer> speedometer =
                    std::shared_ptr<Speedometer>(new IterativeSpeedometer(std::stod(interval), std::stod(alpha),
                        std::stod(alpha_hw), std::stod(speedlim), std::stoi(speedlim_viols)));
                speedometers.insert({interval, speedometer});
            }
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