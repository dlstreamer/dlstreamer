#!/bin/bash
# ==============================================================================
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

# Define the maximum time allowed for web operations
WEB_TIMEOUT=10
APT_UPDATE_TIMEOUT=30
APT_GET_TIMEOUT=600

# Function to handle errors
handle_error() {
    echo -e "\e[31m Error occurred: $1\e[0m"
    exit 1
}

handle_warning() {
    echo -e "\e[33m Error occurred: $1\e[0m"
}

# Function to display text in a given color
echo_color() {
    local text="$1"
    local color="$2"
    local color_code=""

    # Determine the color code based on the color name
    case "$color" in
        black) color_code="\e[30m" ;;
        red) color_code="\e[31m" ;;
        green) color_code="\e[32m" ;;
        bred) color_code="\e[91m" ;;
        bgreen) color_code="\e[92m" ;;
        yellow) color_code="\e[33m" ;;
        blue) color_code="\e[34m" ;;
        magenta) color_code="\e[35m" ;;
        cyan) color_code="\e[36m" ;;
        white) color_code="\e[37m" ;;
        *) echo "Invalid color name"; return 1 ;;
    esac

    # Display the text in the chosen color
    echo -e "${color_code}${text}\e[0m"
}

# Create a temporary directory for downloading .deb files
temp_dir=$(mktemp -d)
echo " Created temporary directory: $temp_dir"

# Cleanup function to remove the temporary directory
cleanup() {
    echo " Cleaning up temporary files..."
    rm -rf "$temp_dir"
}

# Trap to execute the cleanup function on script exit or interrupt
trap cleanup EXIT

# Function to download and install Intel® DL Streamer
install_dl_streamer() {
    
    local ubuntu_version
    ubuntu_version=$1
    local urls_string
    urls_string=$2
    local filter_ubuntu_version
    filter_ubuntu_version=$3

    # Convert the string back into an array
    local download_urls 
    IFS=' ' read -r -a download_urls <<< "$urls_string"

    echo "${download_urls[@]}"

    # Change to the temporary directory
    pushd "$temp_dir" > /dev/null || handle_error "Failed to change to temporary directory"

    echo_color " Downloading Intel® DL Streamer .deb files into temporary directory: $temp_dir" "yellow"

    
    # Loop through the array of URLs and download packages from each one
    for download_url in "${download_urls[@]}"; do
        if [ "$filter_ubuntu_version" = "true" ]; then
            wget -nd --accept-regex=".*ubuntu_${ubuntu_version}\.04.*\.deb" -r --timeout="$WEB_TIMEOUT" --tries=3 "${download_url}" || handle_error "Failed to download Debian packages"
        else
            wget -nd --accept-regex=".*_amd64.deb" -r --timeout="$WEB_TIMEOUT" --tries=3 "${download_url}" || handle_error "Failed to download Debian packages"
        fi
    done

    # Install Intel® DL Streamer using apt
    echo_color " Installing Intel® DL Streamer packages..." "yellow"
    if ! sudo apt install -y --allow-downgrades ./*.deb; then
        echo_color " Attempting to fix broken dependencies..." "yellow"
        sudo apt --fix-broken install || handle_error "Failed to fix broken dependencies"
        # Try the installation again after fixing dependencies
        sudo apt install -y ./*.deb || handle_error "apt failed to install one or more .deb packages after fixing dependencies"
    fi

    # Return to the original directory
    popd > /dev/null || handle_error "Failed to return to original directory"
}

# Function to install OpenVINO™ toolkit
install_openvino() {
    
    # Install OpenVINO™ toolkit (if not installed)
    echo_color " Installing OpenVINO™ toolkit..." "yellow"
    if [ ! -f /opt/intel/dlstreamer/install_dependencies/install_openvino.sh ]; then
        handle_error "OpenVINO™ installation script not found"
    fi
    if ! sudo -E /opt/intel/dlstreamer/install_dependencies/install_openvino.sh; then
        handle_error "Failed to install OpenVINO™ toolkit"
    fi

    echo_color " OpenVINO™ toolkit installed successfully" "green"
}

# Function to ask the user if they want to proceed with Step 2
ask_step_2() {

    echo -e "\n"
    read -p " Do you want to install MQTT and Kafka clients for element gvametapublish? [y/N] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        # Install MQTT and Kafka clients

        search_dir="/usr/local/lib/"
        find "$search_dir" -maxdepth 1 -type l -name 'libpaho-*' -exec sh -c '
        for f; do
            [ -L "$f" ] && echo "Removing $f" && sudo rm "$f"
        done
        ' sh {} +

        if !  sudo -E /opt/intel/dlstreamer/install_dependencies/install_mqtt_client.sh; then
            handle_error "Failed to install MQTT client"
        else 
            echo_color " MQTT client installed successfully." "green"
        fi
        if !  sudo -E /opt/intel/dlstreamer/install_dependencies/install_kafka_client.sh; then
            handle_error "Failed to install Kafka client"
        else
            echo_color " Kafka client installed successfully." "green"
        fi
    else
        echo_color  " Skipping MQTT and Kafka clients installation." "yellow"
    fi
}

# Function to add sourced scripts to .profile
# Function to add sourced scripts to .profile
add_to_profile() {
    local script_path
    script_path="$1"
    local profile
    profile="${HOME}/.profile"
    local bashrc
    bashrc="${HOME}/.bashrc"

    # shellcheck disable=SC1090
    source $script_path

    # Check if the script is already sourced in .profile or .bashrc to avoid duplicates
    if grep -qF -- "$script_path" "$profile"; then
        echo " 'source $script_path' is already in $profile."
    elif grep -qF -- "$script_path" "$bashrc"; then
        echo " 'source $script_path' is already in $bashrc."
    else
        echo "source $script_path" >> "$profile"
        echo " Added 'source $script_path' to $profile."
    fi
}


build_opencv(){

    echo_color " Downloading and building OpenCV library..." "yellow"

    # Change to the temporary directory
    pushd "$temp_dir" > /dev/null || handle_error "Failed to change to temporary directory"

    # Install ninja-build and unzip with error handling
    sudo apt-get install ninja-build unzip || handle_error "Failed to install ninja-build and unzip"

    # Download OpenCV with error handling and timeout
    wget -q --no-check-certificate -O opencv.zip --timeout="$WEB_TIMEOUT" https://github.com/opencv/opencv/archive/4.10.0.zip || handle_error "Failed to download OpenCV or the operation timed out"

    # Unzip OpenCV, remove the zip file, rename the directory, and create a build directory with error handling
    unzip opencv.zip && rm opencv.zip && mv opencv-4.10.0 opencv && mkdir -p opencv/build || handle_error "Failed to unzip OpenCV and set up the directory structure"

    # Change to the OpenCV build directory
    cd ./opencv/build || handle_error "Directory change to ./opencv/build failed."

    # Run CMake and build OpenCV with error handling
    cmake -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_opencv_apps=OFF -GNinja .. || handle_error "CMake configuration failed"
    ninja -j "$(nproc)" || handle_error "OpenCV build failed"
    sudo ninja install || handle_error "OpenCV installation failed"

    # Return to the original directory
    popd > /dev/null || handle_error "Failed to return to original directory"

    echo_color " OpenCV installed successfully" "green"
}

usage() {
    echo "Usage: $0 [-v version] [-u url1] [-u url2] ..."
    exit 1
}

compare_versions() {
    # Split the version strings into arrays based on the '.' delimiter
    IFS='.' read -r -a ver1 <<< "$1"
    IFS='.' read -r -a ver2 <<< "$2"

    # Compare each part of the version strings
    for ((i=0; i<${#ver1[@]}; i++)); do
        # If the corresponding part of ver2 is empty, ver1 is greater (newer)
        if [[ -z ${ver2[i]} ]]; then
            return 1
        fi

        # If the parts are not equal, compare them as integers
        if ((10#${ver1[i]} < 10#${ver2[i]})); then
            return 2
        elif ((10#${ver1[i]} > 10#${ver2[i]})); then
            return 1
        fi
    done

    # If all parts are equal up to the length of ver1, check if ver2 has more parts
    if [[ ${#ver2[@]} -gt ${#ver1[@]} ]]; then
        return 2
    fi

    # Versions are equal
    return 0
}

check_opencv(){
    # Alternatively, check for the presence of OpenCV header files
    opencv_header="/usr/local/include/opencv4/opencv2/core/version.hpp"

    installed_version="0.0.0"

    if [ -f "$opencv_header" ]; then
        # Extract the version from the header file
        major=$(grep -E '^#define CV_VERSION_MAJOR' "$opencv_header" | awk '{print $3}')
        minor=$(grep -E '^#define CV_VERSION_MINOR' "$opencv_header" | awk '{print $3}')
        revision=$(grep -E '^#define CV_VERSION_REVISION' "$opencv_header" | awk '{print $3}')
        installed_version="${major}.${minor}.${revision}"
        echo " OpenCV version $installed_version is installed."
    else
        echo " OpenCV header files not found."
    fi
    
    compare_versions "4.10.0" "$installed_version"
    result=$?

    return $result
}

# ***********************************************************************
# Function to update the package lists with a timeout
update_package_lists() {

    echo_color " Updating package lists..." "yellow"
    
    # Update the package lists with a timeout and capture the output
    local update_output
    update_output=$(timeout --foreground $APT_UPDATE_TIMEOUT sudo apt-get update 2>&1)
    local update_exit_code=$?

    # Display the output
    echo "$update_output"

    # Check for a timeout
    if [ $update_exit_code -eq 124 ]; then
        echo_color " The update process timed out." "bred"
        exit 1  
    fi

    # Check for specific error messages in the output
    if echo "$update_output" | grep -q "403  Forbidden"; then
        echo_color " Repository access was forbidden. Check your repository configuration." "bred"
        exit 1  
    elif echo "$update_output" | grep -q "is not signed"; then
        echo_color " One of the repositories is not signed. Check your repository configuration." "bred"
        exit 1  
    elif [ $update_exit_code -ne 0 ]; then
        echo_color " Failed to update package lists for an unknown reason." "bred"
        exit 1  
    else
        echo_color " Package lists updated successfully." "green"
    fi
}

# ***********************************************************************

install_packages() {
    # Define a temporary file for logging
    local log_file
    log_file=$(mktemp)
    
    echo_color " Installing packages: $*." "yellow"

    # Run apt-get install and use tee to duplicate the output to the log file
    # while still displaying it to the user
    timeout --foreground $APT_GET_TIMEOUT sudo apt-get install "$@" 2>&1 | tee "$log_file"
    local status=${PIPESTATUS[0]}

    # Check the exit status of the apt-get install command
    if [[ $status -eq 124 ]]; then
        handle_error "The command timed out."
    elif [ $status -ne 0 ]; then
        echo_color " An error occurred during package installation." "bred"
        
        # Check for common errors and suggest solutions
        if grep -qi "Unable to fetch some archives" "$log_file"; then
            echo_color " Your package lists may be outdated. Try running 'sudo apt-get update' and then re-run this script." "bred"
        elif grep -qi "Unable to locate package" "$log_file"; then
            echo_color " One or more specified packages could not be found. Check for typos or availability in your current software sources." "yellow"
        elif grep -qi "dpkg was interrupted" "$log_file"; then
            echo_color " The package installation was previously interrupted. Try running 'sudo dpkg --configure -a' to fix this." "yellow"
        else
            echo_color " Check the error messages in the log above to determine the cause of the installation failure." "yellow"
        fi

        # Clean up the log file
        rm -f "$log_file"

        # Exit the script with an error status
        exit 1
    else
        message=" Packages $* installed successfully."
        echo_color "$message" "green"
    fi

    # Clean up the log file
    rm -f "$log_file"
}

#--------------------------------------------------------------------------------------------------------------------------------------------------------

# Initialize variables for named arguments
DLS_VERSION=""
declare -a DOWNLOAD_URLS # Declare an array to hold multiple URLs
RELEASES_URL="https://github.com/dlstreamer/dlstreamer/releases/latest"
LAST_DLS_22="2024.1.1"

echo 
echo_color " Important: Please ensure that all prerequisites have been installed before proceeding." "cyan"
read -p " Have you already run the DLS_install_prerequisites.sh script? [y/N] " -n 1 -r
echo
if [[ $REPLY =~ ^[Nn]$ ]]; then
    echo_color " Please run DLS_install_prerequisites.sh first." "cyan"
    exit
fi

#echo "$DLS_VERSION"
#echo "$DOWNLOAD_URL"
update_package_lists
install_packages curl wget gpg software-properties-common

# Process named arguments
while getopts ":v:u:" opt; do
    case $opt in
        v)
            DLS_VERSION="$OPTARG"
            ;;
        u)
            # Append the URL to the DOWNLOAD_URLS array
            DOWNLOAD_URLS+=("$OPTARG")
            ;;
        \?)
            echo " Invalid option: -$OPTARG" >&2
            usage
            ;;
        :)
            echo " Option -$OPTARG requires an argument." >&2
            usage
            ;;
    esac
done

# Check if at least one URL was provided
if [ ${#DOWNLOAD_URLS[@]} -eq 0 ]; then
    if [ -z "$DLS_VERSION" ]; then
        # Fetch the latest release URL and add it to the DOWNLOAD_URLS array
        latest_url=$(curl -Ls $RELEASES_URL | grep -oE 'include-fragment[^>]+src="([^"]+)"' | grep -oE 'https://github.com/dlstreamer/dlstreamer[^"]+')
        DOWNLOAD_URLS+=("$latest_url")
        DLS_VERSION=$(basename "$latest_url")
        DLS_VERSION="${DLS_VERSION:1}"
    else
        # Construct the download URL for the specified version and add it to the DOWNLOAD_URLS array
        gh_url=$(curl -Ls $RELEASES_URL | grep -oE 'include-fragment[^>]+src="([^"]+)"' | grep -oE 'https://github.com/dlstreamer/dlstreamer[^"]+' | sed 's|/[^/]*$||')
        DOWNLOAD_URLS+=("${gh_url}/v${DLS_VERSION}")
    fi
elif [ -z "$DLS_VERSION" ]; then
    handle_error "You have to provide Intel® DL Streamer version using -v option."
fi


# Step 1: Install Intel® DL Streamer and OpenVINO™ toolkit

echo_color " Installing Intel® DL Streamer version $DLS_VERSION" "yellow"
compare_versions "$LAST_DLS_22" "$DLS_VERSION"
result=$?

BUILD_OPENCV="false"
UBUNTU_24="false"

if [ "$result" -eq 2 ]; then
    # $DLS_VERSION  is greater than "2024.1.1"
    UBUNTU_24="true"

    check_opencv
    result=$?

    if [ "$result" -ne 0 ]; then
        BUILD_OPENCV="true"
    fi
fi

if [ "$BUILD_OPENCV" = "true" ]; then
    install_packages cmake build-essential libpython3-dev python-gi-dev libopencv-dev libgflags-dev libdrm-dev
    build_opencv
fi

# Detect Ubuntu version
ubuntu_version=$(lsb_release -rs)


# Choose the package list based on the Ubuntu version
case "$ubuntu_version" in
    24.04)
        echo_color " Detected Ubuntu version: $ubuntu_version. " "green"

        if [ "$UBUNTU_24" = "false" ]; then
            handle_error "Intel® DL Streamer version $DLS_VERSION is available for Ubuntu 22.04 only." 
        fi

        install_dl_streamer "24" "${DOWNLOAD_URLS[*]}" "true"
        ;;
    22.04)
        echo_color " Detected Ubuntu version: $ubuntu_version. " "green"
        
        if [ "$UBUNTU_24" = "false" ]; then
            install_dl_streamer "22" "${DOWNLOAD_URLS[*]}" "false"
        else
            install_dl_streamer "22" "${DOWNLOAD_URLS[*]}" "true"
        fi
        ;;
    *)
        echo_color " Unsupported Ubuntu version: $ubuntu_version. Exiting." "red"
        exit 1
        ;;
esac

install_openvino

# Step 2: Install MQTT and Kafka clients for element gvametapublish (optional)
ask_step_2

# Step 3: Add user to groups
echo_color " Adding user to video and render groups..." "yellow"
sudo usermod -a -G video "$USER" || handle_error "Failed to add user to video group"
sudo usermod -a -G render "$USER" || handle_warning "Failed to add user to render group"

# Step 4: Set up the environment for Intel® DL Streamer
echo_color " Setting up the environment..." "yellow"

ADD_ENV_TO_PROFILE=0

read -p " Do you want to add the necessary environment variable settings to .profile for future sessions [y/N] " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    add_to_profile "/opt/intel/openvino_2024/setupvars.sh"
    add_to_profile "/opt/intel/dlstreamer/setupvars.sh"
    ADD_ENV_TO_PROFILE=1
else
    source "/opt/intel/openvino_2024/setupvars.sh"
    source "/opt/intel/dlstreamer/setupvars.sh"
    echo_color " The environment variables are removed when you close the shell. Before each run of Intel® DL Streamer, you need to set up the environment with the following commands: \n\t source /opt/intel/openvino_2024/setupvars.sh \n\t source /opt/intel/dlstreamer/setupvars.sh \n." "yellow"
    read -n 1 -s -r -p " Press any key to continue..."
fi


# Step 5: Verify Intel® DL Streamer installation
echo

if gst-inspect-1.0 gvadetect &> /dev/null; then
    echo_color " Intel® DL Streamer verification successful" "green"
else
    handle_error " Intel® DL Streamer verification failed"
    exit 1
fi


# Step 6: Next steps - running sample Intel® DL Streamer pipelines
echo -e "\n--------------------------------------------------------------------------------------"
echo_color " Intel® DL Streamer ${DLS_VERSION} has been installed successfully. You are ready to use it." "bgreen"
echo_color " For further instructions on how to run sample pipeline(s), please refer to the documentation." "white"
if [ "$ADD_ENV_TO_PROFILE" -ne 0 ]; then
    echo_color " IMPORTANT! Please restart the shell for the environment changes to take effect." "cyan"
fi
echo "--------------------------------------------------------------------------------------"


