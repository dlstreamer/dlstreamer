#!/bin/bash
# ==============================================================================
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

# Define the Intel® repository URL, keyring path, and key URL
INTEL_ONEAPI_REPO_URL="https://apt.repos.intel.com/oneapi all main"
INTEL_ONEAPI_KEYRING_PATH="/usr/share/keyrings/intel-sw-products.gpg"
INTEL_ONEAPI_KEY_URL="https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB"
INTEL_ONEAPI_LIST="intel-oneapi.list"


INTEL_DC_GPU_KEY_URL="https://repositories.intel.com/graphics/intel-graphics.key"
INTEL_DC_GPU_REPO_URL="https://repositories.intel.com/graphics/ubuntu jammy flex"

INTEL_CL_GPU_KEY_URL="https://repositories.intel.com/gpu/intel-graphics.key"
INTEL_CL_GPU_REPO_URL="https://repositories.intel.com/gpu/ubuntu jammy client" 

INTEL_GPU_KEYRING_PATH="/usr/share/keyrings/intel-graphics.gpg"


INTEL_GPU_LIST="intel-graphics.list"

CURL_TIMEOUT=10
APT_UPDATE_TIMEOUT=30
APT_GET_TIMEOUT=30

# ***********************************************************************
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
        *) echo "Invalid color name"; echo "$color" ;;
    esac

    # Display the text in the chosen color
    echo -e "${color_code}${text}\e[0m"
}

# ***********************************************************************
# Function to download the Intel® repository GPG key with a timeout
configure_repository() {
    local key_url="$1"
    local new_keyring_path="$2"
    local repo_url="$3"
    local list_name="$4"

    echo "Setting up $repo_url..."

    # Find all files containing the repository entry
    local repo_files
    repo_files=$(grep -rl "^deb .*$repo_url" /etc/apt/sources.list /etc/apt/sources.list.d/)


    # Loop through each file and check for signed-by entries
    for repo_file in $repo_files; do
        # Extract the keyring path from the matching line
        local keyring_path
        keyring_path=$(grep "^deb .*$repo_url" "$repo_file" | grep -oP 'signed-by=\K[^ ]+' | sed 's/]$//' | head -n 1)

        # Check if a keyring path was found
        if [ -n "$keyring_path" ]; then
            echo "Found repository signed with keyring: $keyring_path in $repo_file"

            # Prompt the user for action
            read -p "Do you want to apply the new signature? [y/N] " -n 1 -r
            echo
            if [[ ! $REPLY =~ ^[Yy]$ ]]; then
                echo "Skipping the application of the new signature."
                return 0
            fi
        fi
    done

    # Check if the new keyring file already exists
    if [ -f "$new_keyring_path" ]; then
        # Generate a new keyring file name
        local base_keyring_name
        base_keyring_name=$(basename "$new_keyring_path")
        local keyring_dir
        keyring_dir=$(dirname "$new_keyring_path")
        local new_keyring_name
        new_keyring_name="${base_keyring_name%.*}_$(date +%s).gpg"
        new_keyring_path="$keyring_dir/$new_keyring_name"
        echo "Keyring file already exists. Using new keyring file: $new_keyring_path"
    fi

    echo "Downloading the GPG key..."
    if ! curl -fsSL --connect-timeout $CURL_TIMEOUT $key_url | sudo gpg --yes --dearmor --output "$new_keyring_path"; then
        echo_color "\n Failed to download the GPG key due to a network issue. Exiting the script." "bred"
        exit 1
    fi

    echo "Signing the repository..."
    echo "deb [arch=amd64 signed-by=$new_keyring_path] $repo_url" | sudo tee "/etc/apt/sources.list.d/$list_name" > /dev/null
  
}

# ***********************************************************************
# Function to update the package lists with a timeout
update_package_lists() {
    
    # Update the package lists with a timeout and capture the output
    local update_output
    update_output=$(timeout --foreground $APT_UPDATE_TIMEOUT sudo apt-get update 2>&1)
    local update_exit_code=$?

    # Display the output
    echo "$update_output"

    # Check for a timeout
    if [ $update_exit_code -eq 124 ]; then
        echo_color "The update process timed out." "bred"
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
    
    # Run apt-get install and use tee to duplicate the output to the log file
    # while still displaying it to the user
    sudo apt-get install "$@" 2>&1 | tee "$log_file"
    local status=${PIPESTATUS[0]}

    # Check the exit status of the apt-get install command
    if [ $status -ne 0 ]; then
        echo_color " An error occurred during package installation." "bred"

        # Check for common errors and suggest solutions
        if grep -qi "Unable to fetch some archives" "$log_file"; then
            echo_color " Your package lists may be outdated. Try running 'sudo apt-get update' and then re-run this script." "yellow"
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

# ***********************************************************************

#-----------------------STEP 1-------------------------------------------

# Inform the user about the upcoming package installation
echo_color "\n The script will now update package lists and install required packages." "yellow"

# Install required packages without the -y flag to allow user interaction
if ! timeout --foreground $APT_GET_TIMEOUT sudo apt-get install curl wget gpg software-properties-common jq; then
    echo_color " Package installation was cancelled or timed out. Exiting the script." "bred"
    exit 1
fi

echo_color "\n The packages required to start the installation are ready." "green"

configure_repository "$INTEL_ONEAPI_KEY_URL" "$INTEL_ONEAPI_KEYRING_PATH" "$INTEL_ONEAPI_REPO_URL" "$INTEL_ONEAPI_LIST"

echo_color "\n Intel® One API repository has been configured.\n" "green"

update_package_lists

#-----------------------STEP 2-------------------------------------------

# Initialize the GPU state variable
# 0 - No Intel GPU
# 1 - Intel client GPU
# 2 - Intel Data Center GPU
intel_gpu_state=0
gpu_info=""

# Check for any Intel GPU
intel_gpus=$(lspci -nn | grep -E 'VGA|3D|Display' | grep -i "Intel")

# If Intel GPUs are found, prioritize DC GPU
if [ -n "$intel_gpus" ]; then
    # Check for DC GPU
    intel_dc_gpu=$(echo -e "$intel_gpus" | grep -Ei "0BD5|0BDA|56C0|56C1")
    if [ -n "$intel_dc_gpu" ]; then
        intel_gpu_state=2
        gpu_info="$intel_dc_gpu"
    else
        # If no DC GPU is found, it must be a client GPU
        intel_client_gpu=$(echo -e "$intel_gpus")
        if [ -n "$intel_client_gpu" ]; then
            intel_gpu_state=1
            gpu_info="$intel_client_gpu"
        fi
    fi
fi

# Output the results based on the GPU state
case $intel_gpu_state in
    0)
        echo_color "\n No Intel® GPU detected." "bred"
        ;;
    1)
        echo_color "\n Intel® client GPU detected:" "bgreen"
        echo -e " --------------------------------"
        echo -e " $gpu_info"
        echo -e " --------------------------------"
        ;;
    2)
        echo_color "\n Data Center GPU detected:" "bgreen"
        echo -e " --------------------------------"
        echo -e " $gpu_info"
        echo -e " --------------------------------"
        ;;
esac

if [ $intel_gpu_state -ne 0 ]; then

    # Path to the /dev/dri directory
    DRI_DIR="/dev/dri"

    # Check for the presence of renderD* devices
    render_devices=""
    for file in "$DRI_DIR"/renderD[0-9]*; do
        if [[ -e "$file" && $(basename "$file") =~ ^renderD[0-9]+$ ]]; then
            render_devices+="$file "
        fi
    done

    # Trim the trailing space if any files were found
    render_devices=${render_devices% }

    # If no renderD* devices are found, print a message and exit
    if [ -z "$render_devices" ]; then
        echo_color "\n Intel® GPU hardware is present but kernel drivers were not installed." "yellow"

        case $intel_gpu_state in
        1)
            echo -e "\e[97m To enable GPU support, install the client GPU drivers by following the instructions available at https://dgpu-docs.intel.com/driver/client/overview.html#installing-gpu-packages.\n\e[37m"
            ;;
        2)
            echo -e "\e[97m To enable GPU support, install the Data Center GPU drivers by following the instructions available at https://dgpu-docs.intel.com/driver/installation.html.\n\e[37m"
            ;;
        esac

        intel_gpu_state=0

    else
        echo_color "\n GPU kernel drivers have been found." "green"
    fi

    case $intel_gpu_state in
        1)
            configure_repository "$INTEL_CL_GPU_KEY_URL" "$INTEL_GPU_KEYRING_PATH" "$INTEL_CL_GPU_REPO_URL" "$INTEL_GPU_LIST"
            echo_color "\n Intel® client GPU repository has been configured.\n" "green"
            ;;
        2)
            configure_repository "$INTEL_DC_GPU_KEY_URL" "$INTEL_GPU_KEYRING_PATH" "$INTEL_DC_GPU_REPO_URL" "$INTEL_GPU_LIST"
            echo_color "\n Intel® Data Center GPU repository has been configured.\n" "green"
            ;;
    esac

    if [ $intel_gpu_state -ne 0 ]; then
        update_package_lists
        install_packages intel-level-zero-gpu level-zero
    fi

fi

#-----------------------STEP 3-------------------------------------------

intel_npu=$(lspci | grep -i 'Intel' | grep 'NPU' | rev | cut -d':' -f1 | rev)

if [ -n "$intel_npu" ]; then
    echo_color "\n Intel® NPU detected:" "green"
    echo -e " --------------------------------"
    echo -e " $intel_npu"
    echo -e " --------------------------------"

    intel_npu_state=$(sudo dmesg | grep -i "initialized intel_vpu")

    if [ -n "$intel_npu_state" ]; then
        echo_color "\n NPU kernel drivers have been found."

        line_to_add="export ZE_ENABLE_ALT_DRIVERS=libze_intel_vpu.so"

        # Define the .bash_profile file path for the current user
        bash_profile="${HOME}/.bash_profile"

        # Check if .bash_profile exists, create it if it does not
        if [ ! -f "$bash_profile" ]; then
            # If .bash_profile does not exist, check for .profile
            if [ ! -f "${HOME}/.profile" ]; then
                # Neither .bash_profile nor .profile exists, create .bash_profile
                touch "$bash_profile"
            else
                # .profile exists, so use that instead
                bash_profile="${HOME}/.profile"
            fi
        fi

        # Check if the line already exists in .bash_profile to avoid duplicates
        if ! grep -qF -- "$line_to_add" "$bash_profile"; then
            # If the line does not exist, append it to .bash_profile
            echo "$line_to_add" >> "$bash_profile"
            # shellcheck disable=SC1090
            source "$bash_profile"
        fi

    else
        echo_color "\n Intel® NPU hardware is present but kernel drivers were not installed." "yellow"
        echo  " To enable NPU support, follow the instructions available at https://github.com/intel/linux-npu-driver/releases"
    fi
fi

echo_color "\n Environment setup completed successfully. " "bgreen"
echo -e " You may now proceed with the installation of Intel® DL Streamer. \e[37m"

echo " ---------------------------------------------------"
echo  " The following hardware will be enabled: "
echo  " - CPU ($(lscpu | grep "Model name" | sed 's/Model name:[[:space:]]*//')) "

if [ $intel_gpu_state -ne 0 ]; then
    short_gpu_info=$(echo "$gpu_info" | grep -o "Intel.*")
    echo " - GPU ($short_gpu_info)"
fi

if [ -n "$intel_npu" ]; then
    echo " - NPU ($intel_npu)"
fi

echo " ---------------------------------------------------"

if [ "$intel_gpu_state" -eq 0 ]; then
    echo_color "\n If you believe that a GPU should be enabled on your system, please install the appropriate drivers and rerun the script. " "yellow"
fi
if [  -z "$intel_npu"  ]; then
    echo_color "\n If you believe that an NPU should be enabled on your system, please install the appropriate drivers and rerun the script. " "yellow"
fi

