/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

/**
 * @file gvarealsense_utils.cpp
 * @brief Utility functions for RealSense device detection and information gathering.
 *
 * This file provides helper functions to check device availability, enumerate
 * RealSense devices and their sensors, and print device information for debugging.
 * It interacts with the librealsense2 library and system utilities to facilitate
 * RealSense device management in GStreamer elements.
 */

#include <fcntl.h>
#include <gst/gst.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "gvarealsense.h"
#include "gvarealsense_utils.h"
#include <glib/gstdio.h>

#define struct_stat struct stat

/**
 * @brief Checks if a RealSense device is available at the specified device path.
 *
 * This function verifies the existence of a device by checking if the given
 * device path exists in the filesystem. If the path is valid and accessible,
 * the function returns TRUE; otherwise, it returns FALSE and prints an error
 * message to the standard output.
 *
 * @param devPath The path to the device (as a gchar*).
 * @return TRUE if the device is available, FALSE otherwise.
 */
gboolean gva_real_sense_is_device_available(gchar *devPath) {
    gboolean bRet = FALSE;

    if (!devPath) {
        GST_ERROR("gva_real_sense_is_device_available: devPath is NULL\n");
        return bRet;
    }

    struct_stat st;
    if (stat(devPath, &st) == 0) {
        bRet = TRUE;
    } else {
        GST_ERROR("gva_real_sense_is_device_available: stat failed for %s, error: %s\n", devPath, strerror(errno));
    }

    return bRet;
} // gva_real_sense_is_device_available

/**
 * @brief Detects and gathers information about sensors in a given RealSense device.
 *
 * This function queries all sensors present in the provided RealSense device (rsDev),
 * extracts their names, and iterates through each sensor's supported stream profiles.
 * For each profile, it collects the profile name, format, resolution, and frame rate,
 * and stores this information in the provided rsDeviceInfo structure (device).
 * The function logs debug information for each detected sensor profile.
 *
 * @param rsDev Reference to the RealSense device to inspect.
 * @param device Reference to the rsDeviceInfo structure where sensor information will be stored.
 * @return TRUE if sensors are detected and information is gathered successfully, FALSE otherwise.
 */
gboolean detectSensorsInRSDevice(rs2::device &rsDev, rsDeviceInfo &device) {
    gboolean bRet = FALSE;

    if (!rsDev) {
        GST_ERROR("detectSensorsInRSDevice: rsDev is NULL\n");
        return bRet;
    }

    auto sensors = rsDev.query_sensors();
    for (auto &&sensor : sensors) {
        rsSensorsInfo sensorInfo;
        sensorInfo.name = sensor.get_info(RS2_CAMERA_INFO_NAME);
        sensorInfo.formats.clear();
        sensorInfo.resolutions.clear();
        sensorInfo.fps.clear();

        auto stream_profiles = sensor.get_stream_profiles();
        for (auto &&profile : stream_profiles) // rs2::stream_profile
        {
            rsProfilesInfo profileInfo;
            profileInfo.name = profile.stream_name();
            profileInfo.formats.push_back(rs2_format_to_string(profile.format()));
            profileInfo.resolutions.push_back(
                {profile.as<rs2::video_stream_profile>().width(), profile.as<rs2::video_stream_profile>().height()});
            profileInfo.fps.push_back(profile.fps());
            sensorInfo.profiles.push_back(profileInfo);

            GST_CAT_DEBUG(gst_real_sense_debug, "Sensor: %s, Profile: %s, Format: %s, Resolution: %dx%d, FPS: %d\n",
                          sensorInfo.name.c_str(), profileInfo.name.c_str(), profileInfo.formats.back().c_str(),
                          profileInfo.resolutions.back().first, profileInfo.resolutions.back().second,
                          profileInfo.fps.back());
        }

        device.sensors.push_back(sensorInfo);
    }

    bRet = true;
    return bRet;
} // detectSensorsInRSDevice

/**
 * @brief Detects all connected RealSense devices and gathers their information.
 *
 * This function queries the system for all connected RealSense devices using librealsense.
 * For each detected device, it retrieves various device properties such as serial number,
 * firmware versions, physical port, debug information, product details, and more. It then
 * calls detectSensorsInRSDevice() to gather information about the sensors and their profiles
 * for each device. Successfully detected devices with their sensor information are added to
 * the provided deviceList structure.
 *
 * @param deviceList Reference to a _rsDeviceList structure where detected device information will be stored.
 * @return TRUE if device detection was performed (regardless of whether devices were found), FALSE otherwise.
 */
gboolean detectRealSenseDevices(_rsDeviceList &deviceList) {
    gboolean bRet = FALSE;
    rs2::context ctx;
    rs2::device_list devices = ctx.query_devices();

    for (auto &&dev : devices) {
        rsDeviceInfo deviceInfo;
        deviceInfo.serial_number = dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
        deviceInfo.firmware_version = dev.get_info(RS2_CAMERA_INFO_FIRMWARE_VERSION);
        deviceInfo.recommended_firmware_version = dev.get_info(RS2_CAMERA_INFO_RECOMMENDED_FIRMWARE_VERSION);
        deviceInfo.physical_port = dev.get_info(RS2_CAMERA_INFO_PHYSICAL_PORT);
        deviceInfo.debug_op_code = dev.get_info(RS2_CAMERA_INFO_DEBUG_OP_CODE);
        deviceInfo.advanced_mode = dev.get_info(RS2_CAMERA_INFO_ADVANCED_MODE);
        deviceInfo.product_id = dev.get_info(RS2_CAMERA_INFO_PRODUCT_ID);
        deviceInfo.camera_locked = dev.get_info(RS2_CAMERA_INFO_CAMERA_LOCKED);
        deviceInfo.usb_type_descriptor = dev.get_info(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR);
        deviceInfo.product_line = dev.get_info(RS2_CAMERA_INFO_PRODUCT_LINE);
        deviceInfo.asic_serial_number = dev.get_info(RS2_CAMERA_INFO_ASIC_SERIAL_NUMBER);
        deviceInfo.firmware_update_id = dev.get_info(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID);
        // deviceInfo.ip_address = dev.get_info(RS2_CAMERA_INFO_IP_ADDRESS); // Uncomment if needed
        deviceInfo.dfu_device_path = dev.get_info(RS2_CAMERA_INFO_DFU_DEVICE_PATH);

        // Detect sensors in the device
        if (!detectSensorsInRSDevice(dev, deviceInfo)) {
            GST_ERROR("detectRealSenseDevices: Failed to detect sensors in device %s\n",
                      deviceInfo.serial_number.c_str());
            // Add the device info to the list

        } else {
            deviceList.devices.push_back(deviceInfo);
            g_print("detectRealSenseDevices: Successfully detected device %s with %zu sensors.\n",
                    deviceInfo.serial_number.c_str(), deviceInfo.sensors.size());
        } // if
    }

    bRet = true;
    return bRet;
} // detectRealSenseDevices

/**
 * @brief Dumps detailed information about all detected RealSense devices.
 *
 * This function prints to the standard output a formatted summary of each RealSense device
 * found in the provided device list. For each device, it displays properties such as serial number,
 * firmware versions, physical port, debug information, product details, and more. Additionally,
 * it iterates through all sensors and their supported profiles, printing out the profile name,
 * format, resolution, and frame rate for each.
 *
 * @param deviceList Reference to a _rsDeviceList structure containing the list of detected devices.
 *
 * Example output includes device metadata and sensor profile details for debugging or informational purposes.
 */
void dumpRealSenseDevices(_rsDeviceList &deviceList) {
    g_print("=================================================\n");
    g_print("====  Dumping RealSense Devices Information: ====\n");

    if (deviceList.devices.empty()) {
        g_print("No RealSense devices found.\n");
        return;
    }

    g_print("Detected RealSense devices:\n");
    for (const auto &device : deviceList.devices) {
        g_print("Serial Number: %s\n", device.serial_number.c_str());
        g_print("Firmware Version: %s\n", device.firmware_version.c_str());
        g_print("Recommended Firmware Version: %s\n", device.recommended_firmware_version.c_str());
        g_print("Physical Port: %s\n", device.physical_port.c_str());
        g_print("Debug Op Code: %s\n", device.debug_op_code.c_str());
        g_print("Advanced Mode: %s\n", device.advanced_mode.c_str());
        g_print("Product ID: %s\n", device.product_id.c_str());
        g_print("Camera Locked: %s\n", device.camera_locked.c_str());
        g_print("USB Type Descriptor: %s\n", device.usb_type_descriptor.c_str());
        g_print("Product Line: %s\n", device.product_line.c_str());
        g_print("ASIC Serial Number: %s\n", device.asic_serial_number.c_str());
        g_print("Firmware Update ID: %s\n", device.firmware_update_id.c_str());
        // g_print("IP Address: %s\n", device.ip_address.c_str()); // Uncomment if needed
        g_print("DFU Device Path: %s\n", device.dfu_device_path.c_str());

        for (const auto &sensor : device.sensors) {
            g_print("\tSensor Name: %s\n", sensor.name.c_str());
            for (const auto &profile : sensor.profiles) {
                for (size_t i = 0; i < profile.formats.size(); ++i) {
                    g_print("\t\tProfile Name: %s, Format: %s, Resolution: %dx%d, FPS: %d\n", profile.name.c_str(),
                            profile.formats[i].c_str(), profile.resolutions[i].first, profile.resolutions[i].second,
                            profile.fps[i]);
                }
            }
        }
        g_print("-------------------------------------------------\n");
    }
    g_print("Total devices found: %zu\n", deviceList.devices.size());
} // dumpRealSenseDevices

/**
 * @brief Checks if a given device is available via udev and matches a connected RealSense device.
 *
 * This function executes the `udevadm info` command to retrieve udev information for the specified device,
 * then queries all connected RealSense devices using librealsense. It attempts to match the udev information
 * with the physical port information of each RealSense device. If a match is found, the function returns TRUE.
 *
 * @param device The device name (e.g., "/dev/video0") to check for udev availability.
 * @return TRUE if the device is available and matches a connected RealSense device, FALSE otherwise.
 */
gboolean is_rs_device_available(const char *device) {
    gboolean bRet = false;
    char command[256];
    char udev_info[1024];
    size_t udev_info_size = sizeof(udev_info);

    if (!device || strlen(device) == 0) {
        g_print("is_rs_device_available: Device is NULL or empty\n");
        return bRet;
    }

    snprintf(command, sizeof(command), "udevadm info --query=all --name=%s", device);
    FILE *pipe = popen(command, "r");
    if (!pipe) {
        g_print("Failed to run udevadm\n");
        return false;
    }
    if (!fgets(udev_info, udev_info_size, pipe)) {
        GST_DEBUG("is_rs_device_available: Failed to read udev info for device %s\n", device);
        pclose(pipe);
        return bRet;
    }

    pclose(pipe);

    // Get information about RealSense device
    rs2::context ctx;
    auto devices = ctx.query_devices();
    std::string udev_info_str(udev_info);

    if (udev_info_str.empty()) {
        g_print("is_rs_device_available: udev info is empty for device %s\n", device);
        return bRet;
    }

    for (auto &&dev : devices) {
        std::string physical_port = dev.get_info(RS2_CAMERA_INFO_PHYSICAL_PORT);

        if (physical_port.empty()) {
            g_print("is_rs_device_available: Physical port is empty for device %s\n",
                    dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER));
            continue;
        }

        size_t pos = udev_info_str.find('/');
        udev_info_str.erase(0, pos); // Remove the leading part before the first '/'

        g_print("Checking device: %s \nagainst udev info: %s\n", physical_port.c_str(), udev_info_str.c_str());

        if (!udev_info_str.empty())
            udev_info_str.pop_back(); // remove last character if it's a newline

        if (physical_port.find(udev_info_str) != std::string::npos) {
            bRet = true;
            break;
        }
    }

    return bRet;
} // is_rs_device_available

/**
 * @brief Builds a buffer containing point cloud data in PCD (Point Cloud Library) format for x, y, z, r, g, b
 * variables.
 *
 * This function constructs a PCD file buffer (ASCII format) from provided vectors of x, y, z coordinates and r, g, b
 * color values. The output buffer can be saved directly as a .pcd file or used for further processing.
 *
 * @param x Vector of x coordinates.
 * @param y Vector of y coordinates.
 * @param z Vector of z coordinates.
 * @param r Vector of red color values (0-255).
 * @param g Vector of green color values (0-255).
 * @param b Vector of blue color values (0-255).
 * @return std::string containing the PCD file content.
 */
std::string build_pcd_buffer(const std::vector<float> &x, const std::vector<float> &y, const std::vector<float> &z,
                             const std::vector<uint8_t> &r, const std::vector<uint8_t> &g,
                             const std::vector<uint8_t> &b) {
    size_t num_points = x.size();
    if (y.size() != num_points || z.size() != num_points || r.size() != num_points || g.size() != num_points ||
        b.size() != num_points) {
        g_print("build_pcd_buffer: Input vector sizes do not match\n");
        return "";
    }

    std::ostringstream oss;
    oss << "# .PCD v0.7 - Point Cloud Data file format\n";
    oss << "VERSION 0.7\n";
    oss << "FIELDS x y z r g b\n";
    oss << "SIZE 4 4 4 1 1 1\n";
    oss << "TYPE F F F U U U\n";
    oss << "COUNT 1 1 1 1 1 1\n";
    oss << "WIDTH " << num_points << "\n";
    oss << "HEIGHT 1\n";
    oss << "VIEWPOINT 0 0 0 1 0 0 0\n";
    oss << "POINTS " << num_points << "\n";
    oss << "DATA ascii\n";

    for (size_t i = 0; i < num_points; ++i) {
        oss << x[i] << " " << y[i] << " " << z[i] << " " << static_cast<int>(r[i]) << " " << static_cast<int>(g[i])
            << " " << static_cast<int>(b[i]) << "\n";
    }

    return oss.str();
}