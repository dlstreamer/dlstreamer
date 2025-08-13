/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __GST_REAL_SENSE_UTILS_H__
#define __GST_REAL_SENSE_UTILS_H__

#include <glib.h>
#include <gst/gst.h>

#include <vector>

#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API

struct rsProfilesInfo {
    std::string name;
    std::vector<std::string> formats;             // List of supported formats
    std::vector<std::pair<int, int>> resolutions; // List of supported resolutions (width, height)
    std::vector<int> fps;                         // List of supported frames per second
};

struct rsSensorsInfo {
    std::string name;
    std::vector<std::string> formats;             // List of supported formats
    std::vector<std::pair<int, int>> resolutions; // List of supported resolutions (width, height)
    std::vector<int> fps;                         // List of supported frames per second

    std::vector<rsProfilesInfo> profiles; // List of profiles for the sensor
};

struct rsDeviceInfo {
    std::string serial_number;
    std::string firmware_version;
    std::string recommended_firmware_version;
    std::string physical_port;
    std::string debug_op_code;
    std::string advanced_mode;
    std::string product_id;
    std::string camera_locked;
    std::string usb_type_descriptor;
    std::string product_line;
    std::string asic_serial_number;
    std::string firmware_update_id;
    // std::string ip_address; // Uncomment if needed
    std::string dfu_device_path;

    std::vector<rsSensorsInfo> sensors; // List of sensors associated with the device
};

typedef struct {
    std::vector<rsDeviceInfo> devices; // List of devices with their information
} _rsDeviceList;

gboolean detectRealSenseDevices(_rsDeviceList &deviceList);
gboolean detectSensorsInRSDevice(rs2::device &rsDev, rsDeviceInfo &device);

gboolean isRealSenseDeviceAvailable(const std::string &devPath, _rsDeviceList &deviceList);

void dumpRealSenseDevices(_rsDeviceList &deviceList);

gboolean is_rs_device_available(const char *device);

#endif /* __GST_REAL_SENSE_UTILS_H__ */
