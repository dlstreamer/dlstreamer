#!/bin/bash
# ==============================================================================
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

# Define the maximum time allowed for web operations
WEB_TIMEOUT=10

# Function to handle errors
handle_error() {
    echo -e "\e[31mError occurred: $1\e[0m"
    exit 1
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
echo "Created temporary directory: $temp_dir"

# Cleanup function to remove the temporary directory
cleanup() {
    echo "Cleaning up temporary files..."
    rm -rf "$temp_dir"
}

# Trap to execute the cleanup function on script exit or interrupt
trap cleanup EXIT

# Function to download and install Intel® DL Streamer
install_dl_streamer() {

    # Change to the temporary directory
    pushd "$temp_dir" > /dev/null || handle_error "Failed to change to temporary directory"

    echo_color "Downloading Intel® DL Streamer .deb files into temporary directory: $temp_dir" "yellow"

    # Use wget to download all .deb files from the specified URL with timeout handling
    wget -q -nH --accept-regex=".*\.deb" --cut-dirs=5 -r --timeout="$WEB_TIMEOUT" --tries=3 https://github.com/dlstreamer/dlstreamer/releases/expanded_assets/v2024.1.1 || handle_error "Failed to download Debian packages"

    # Install Intel® DL Streamer using apt
    echo_color "Installing Intel® DL Streamer..." "yellow"
    if ! sudo apt install -y ./*.deb; then
        echo_color "Attempting to fix broken dependencies..." "yellow"
        sudo apt --fix-broken install || handle_error "Failed to fix broken dependencies"
        # Try the installation again after fixing dependencies
        sudo apt install -y ./*.deb || handle_error "apt failed to install one or more .deb packages after fixing dependencies"
    fi

    # Install OpenVINO™ toolkit (if not installed)
    echo_color "Installing OpenVINO™ toolkit..." "yellow"
    if [ ! -f /opt/intel/dlstreamer/install_dependencies/install_openvino.sh ]; then
        handle_error "OpenVINO™ installation script not found"
    fi
    if ! sudo -E /opt/intel/dlstreamer/install_dependencies/install_openvino.sh; then
        handle_error "Failed to install OpenVINO™ toolkit"
    fi

    # Return to the original directory
    popd > /dev/null || handle_error "Failed to return to original directory"
}

# Function to install OpenVINO™ toolkit
install_openvino() {
    
    # Install OpenVINO™ toolkit (if not installed)
    echo_color "Installing OpenVINO™ toolkit..." "yellow"
    if [ ! -f /opt/intel/dlstreamer/install_dependencies/install_openvino.sh ]; then
        handle_error "OpenVINO™ installation script not found"
    fi
    if ! sudo -E /opt/intel/dlstreamer/install_dependencies/install_openvino.sh; then
        handle_error "Failed to install OpenVINO™ toolkit"
    fi

}

# Function to ask the user if they want to proceed with Step 2
ask_step_2() {

    echo "\n"
    read -p "Do you want to install MQTT and Kafka clients for element gvametapublish? [y/N] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        # Install MQTT and Kafka clients
        if !  sudo -E /opt/intel/dlstreamer/install_dependencies/install_mqtt_client.sh; then
            handle_error "Failed to install MQTT client"
        else 
            echo_color "MQTT client installed successfully." "green"
        fi
        if !  sudo -E /opt/intel/dlstreamer/install_dependencies/install_kafka_client.sh; then
            handle_error "Failed to install Kafka client"
        else
            echo_color "Kafka client installed successfully." "green"
        fi
    else
        echo_color  "Skipping MQTT and Kafka clients installation." "yellow"
    fi
}

# Function to add sourced scripts to .profile
# Function to add sourced scripts to .profile
add_to_profile() {
    local script_path="$1"
    local profile="${HOME}/.profile"
    local bashrc="${HOME}/.bashrc"

    # shellcheck disable=SC1090
    source $script_path

    # Check if the script is already sourced in .profile or .bashrc to avoid duplicates
    if grep -qF -- "$script_path" "$profile"; then
        echo "'source $script_path' is already in $profile."
    elif grep -qF -- "$script_path" "$bashrc"; then
        echo "'source $script_path' is already in $bashrc."
    else
        # If the script is not sourced, ask the user if they want to add it to .profile
        read -p "Do you want to add 'source $script_path' to $profile for future sessions? [y/N] " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            # Add the script to .profile
            echo "source $script_path" >> "$profile"
            echo "Added 'source $script_path' to $profile."
        else
            echo "Skipping adding 'source $script_path' to $profile."
        fi
    fi
}

# Step 1: Install Intel® DL Streamer and OpenVINO™ toolkit
install_dl_streamer
install_openvino

# Step 2: Install MQTT and Kafka clients for element gvametapublish (optional)
ask_step_2

# Step 3: Add user to groups
echo_color "Adding user to video and render groups..." "yellow"
sudo usermod -a -G video "$USER" || handle_error "Failed to add user to video group"
sudo usermod -a -G render "$USER" || handle_error "Failed to add user to render group"

# Step 4: Set up the environment for Intel® DL Streamer
echo_color "Setting up the environment..." "yellow"
add_to_profile "/opt/intel/openvino_2024/setupvars.sh"
add_to_profile "/opt/intel/dlstreamer/gstreamer/setupvars.sh"
add_to_profile "/opt/intel/dlstreamer/setupvars.sh"
# Step 5: Verify Intel® DL Streamer installation

if gst-inspect-1.0 gvadetect &> /dev/null; then
    echo_color "Intel® DL Streamer verification successful" "green"
else
    handle_error "Intel® DL Streamer verification failed"
    exit 1
fi


# Step 6: Next steps - running sample Intel® DL Streamer pipelines
echo -e "\n--------------------------------------------------------------------------------------"
echo_color "Intel® DL Streamer has been installed successfully. You are ready to use it." "bgreen"
echo_color "For further instructions to run sample pipeline(s), please refer to the documentation." "white"
echo "--------------------------------------------------------------------------------------"


