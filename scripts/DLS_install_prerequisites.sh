#!/bin/bash
# ==============================================================================
# Copyright (C) 2024-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

# Define the Intel® repository URL, keyring path, and key URL
INTEL_ONEAPI_REPO_URL="https://apt.repos.intel.com/oneapi all main"
INTEL_ONEAPI_KEYRING_PATH="/usr/share/keyrings/intel-sw-products.gpg"
INTEL_ONEAPI_KEY_URL="https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB"
INTEL_ONEAPI_LIST="intel-oneapi.list"


INTEL_DC_GPU_KEY_URL="https://repositories.intel.com/graphics/intel-graphics.key"
INTEL_DC_GPU_REPO_URL_22="https://repositories.intel.com/graphics/ubuntu jammy flex"
INTEL_DC_GPU_REPO_URL_24="https://repositories.intel.com/graphics/ubuntu noble flex"

INTEL_CL_GPU_KEY_URL="https://repositories.intel.com/gpu/intel-graphics.key"
INTEL_CL_GPU_REPO_URL_22="https://repositories.intel.com/gpu/ubuntu jammy client" 
INTEL_CL_GPU_REPO_URL_24="https://repositories.intel.com/gpu/ubuntu noble client" 

INTEL_GPU_KEYRING_PATH="/usr/share/keyrings/intel-graphics.gpg"


INTEL_GPU_LIST="intel-graphics.list"

CURL_TIMEOUT=60
APT_UPDATE_TIMEOUT=60
APT_GET_TIMEOUT=600

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

# Function to handle errors
handle_error() {
    echo -e "\e[31mError occurred: $1\e[0m"
    exit 1
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
    
    echo_color " Installing packages: $*." "yellow"

    # Run apt-get install and use tee to duplicate the output to the log file
    # while still displaying it to the user
    timeout --foreground $APT_GET_TIMEOUT sudo apt-get install -y "$@" 2>&1 | tee "$log_file"
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
        message=" Installed $* successfully."
        echo_color "$message" "green"
    fi

    # Clean up the log file
    rm -f "$log_file"
}

# Function to check if the kernel version is 6.12 or higher
check_kernel_version() {
    # Get the current kernel version
    current_kernel=$(uname -r)

    # Extract the major and minor version numbers
    current_major=$(echo $current_kernel | cut -d. -f1)
    current_minor=$(echo $current_kernel | cut -d. -f2)

    # Define the target major and minor version numbers
    target_major=6
    target_minor=12

    # Function to compare version numbers
    version_ge() {
        [ "$1" -gt "$2" ] || { [ "$1" -eq "$2" ] && [ "$3" -ge "$4" ]; }
    }

    # Check if the current kernel version is greater than or equal to the target version
    if version_ge $current_major $target_major $current_minor $target_minor; then
        return 0  # Success
    else
        return 1  # Failure
    fi
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

# Function to download a package
download_deb_package() {
    local package_url=$1
    local timeout_value=$2

    # Extract the package name from the URL
    local package_name
    package_name=$(basename "$package_url")

    # Download the package with the specified timeout
    echo "Downloading $package_name..."
    wget --timeout="$timeout_value" -O "$package_name" "$package_url"

    # Check if the download was successful
    if [ $? -eq 0 ]; then
        echo_color " Downloaded $package_name successfully." "green"
    else
        handle_error " Failed to download $package_name."
    fi
}

# Function to download and install a package
install_deb_package() {
    local package_url=$1
    local timeout_value=$2

    # Extract the package name from the URL
    local package_name
    package_name=$(basename "$package_url")

    if ! sudo -E apt install -y --allow-downgrades ./"$package_name"; then
        echo_color " Attempting to fix broken dependencies..." "yellow"
        sudo apt --fix-broken install || handle_error "Failed to fix broken dependencies"
        # Try the installation again after fixing dependencies
        sudo apt install -y ./"$package_name" || handle_error "apt failed to install $package_name after fixing dependencies"
    else 
        echo_color " Installed $package_name successfully. \n" "green"
    fi
}


setup_gpu(){

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

    update_package_lists
    install_packages libze1 intel-level-zero-gpu intel-opencl-icd clinfo

}

# Function to get .deb package URLs from the latest release of a GitHub repository
get_deb_urls() {
    local REPO=$1

    # Get the latest release information using the GitHub API
    local LATEST_RELEASE
    LATEST_RELEASE=$(curl -s "https://api.github.com/repos/$REPO/releases/latest")

    # Extract the URLs of the .deb packages
    local DEB_URLS
    DEB_URLS=$(echo "$LATEST_RELEASE" | grep browser_download_url | grep -Eo 'https://[^"]+\.deb' | sed 's/%2B/+/g')

    # Return the list of .deb package URLs
    echo "$DEB_URLS"
}

# Function to get the URLs of .deb packages from a GitHub release page
get_deb_urls_no_api() {
  local repo="$1"
  local tag="$2"
  local filter="$3"
  local release_url="https://github.com/$repo/releases/tag/$tag"
  local expanded_assets_url
  local deb_urls

  # Fetch the HTML content of the release page
  html_content=$(curl -s "$release_url")

  # Extract the URL of the expanded assets
  expanded_assets_url=$(echo "$html_content" | grep -oP 'include-fragment[^>]+src="\K[^"]+' | grep 'expanded_assets')

  # Check if the expanded_assets_url starts with "https"
  if [[ "$expanded_assets_url" != https* ]]; then
    expanded_assets_url="https://github.com$expanded_assets_url"
  fi

  # Fetch the HTML content of the expanded assets page
  expanded_assets_content=$(curl -s "$expanded_assets_url")

  # Extract the URLs of .deb files
  #deb_urls=$(echo "$expanded_assets_content" | grep -oP 'href="\K[^"]+\.deb' | sed 's|^|https://github.com|')

  if [ -n "$filter" ]; then
    deb_urls=$(echo "$expanded_assets_content" | grep -oP 'href="\K[^"]+\.deb' | grep "$filter" | grep -v "devel" | sed 's|^|https://github.com|')
  else
    deb_urls=$(echo "$expanded_assets_content" | grep -oP 'href="\K[^"]+\.deb' | sed 's|^|https://github.com|')
  fi

  echo "$deb_urls"
}

# Function to get the URLs of .deb packages from a GitHub release page
get_deb_urls_wget() {
  local repo="$1"
  local tag="$2"
  local filter="$3"
  local release_url="https://github.com/$repo/releases/tag/$tag"
  local deb_urls

  # Fetch the HTML content of the release page
  html_content=$(curl -s "$release_url")

  # Extract the URLs of .deb files from lines starting with wget
  if [ -n "$filter" ]; then
    deb_urls=$(echo "$html_content" | grep -oP 'wget \Khttps://[^ ]+\.deb' | grep "$filter")
  else
    deb_urls=$(echo "$html_content" | grep -oP 'wget \Khttps://[^ ]+\.deb')
  fi

  echo "$deb_urls"
}

# Function to check if the current user is a member of a given group and add if not
add_user_to_group_if_not_member() {
  local group="$1"
  local user="$USER"

  # Check if the group exists
  if ! getent group "$group" > /dev/null; then
    handle_error " Group '$group' does not exist."
  fi

  # Check if the user is a member of the group
  if id -nG "$user" | tr ' ' '\n' | grep -q "^$group$"; then
    echo_color " The user $user is already a member of the group $group." "green"
    return 0
  else
    # Add the user to the group
    sudo usermod -aG "$group" "$user"
    if [ $? -eq 0 ]; then
      echo_color " The user $user has been added to the group $group." "cyan"
      return 1
    else
      handle_error  " Failed to add the user $user to the group $group."
    fi
  fi
}

# Function to get the installed version of a package
get_installed_version() {
  local package_name="$1"
  dpkg-query -W -f='${Version}' "$package_name" 2>/dev/null
}

# Function to check the version of a package
check_package_version() {
  local package_name="$1"

  # Check if the package is installed
  if dpkg -l "$package_name" 2>/dev/null | grep -q "^ii"; then
    # Get the package version
    package_version=$(dpkg -l "$package_name" | grep "^ii" | awk '{print $3}')
    echo "The version of the package '$package_name' is: $package_version"
  else
    echo "The package '$package_name' is not installed."
  fi
}

# Function to get the latest version of a package from GitHub by following the redirect
get_latest_version_github() {

  local repo="$1"
  local latest_url="https://github.com/$repo/releases/latest"
  local redirect_url

  # Use curl to follow the redirect and extract the final URL
  redirect_url=$(curl -s -L -o /dev/null -w '%{url_effective}' "$latest_url")

  # Extract the version tag from the final URL
  latest_version=$(basename "$redirect_url")

  # Remove the leading "v" if present
  #latest_version="${latest_version#v}"

  echo "$latest_version"
}

update_gpu_setup() {

    # Change to the temporary directory
    pushd "$temp_dir" > /dev/null || handle_error "Failed to change to temporary directory"

    #REPO1="intel/intel-graphics-compiler"
    REPO2="intel/compute-runtime"

    #TAG1=$(get_latest_version_github "$REPO1")
    TAG2=$(get_latest_version_github "$REPO2")

    # Get .deb package URLs for each repository
    #DEB_URLS1=$(get_deb_urls_no_api $REPO1 $TAG1)
    DEB_URLS2=$(get_deb_urls_wget $REPO2 $TAG2)

    # Merge the results into a single array
    package_urls=()
    #while IFS= read -r url; do
    #    package_urls+=("$url")
    #done <<< "$DEB_URLS1"

    while IFS= read -r url; do
        package_urls+=("$url")
    done <<< "$DEB_URLS2"

    # Find the URL that contains "libigdgmm" and move it to the first position (as other packages rely on it)
    for i in "${!package_urls[@]}"; do
        if [[ "${package_urls[$i]}" == *"libigdgmm"* ]]; then
            # Store the URL
            libigdgmm_url="${package_urls[$i]}"
            
            # Remove the URL from its current position
            unset 'package_urls[$i]'
            
            # Insert the URL at the beginning of the array
            package_urls=("$libigdgmm_url" "${package_urls[@]}")
            
            # Break the loop as we have found and moved the URL
            break
        fi
    done

    # Iterate over the list of package URLs and download each one
    for package_url in "${package_urls[@]}"; do
        download_deb_package "$package_url" "$APT_GET_TIMEOUT"
        install_deb_package "$package_url" "$APT_GET_TIMEOUT"
    done
    
    
    # Return to the original directory
    popd > /dev/null || handle_error "Failed to return to original directory"
}


setup_npu() {

    # Change to the temporary directory
    pushd "$temp_dir" > /dev/null || handle_error "Failed to change to temporary directory"

    update_package_lists
    install_packages libtbb12
    
    REPO1="intel/linux-npu-driver"
    REPO2="oneapi-src/level-zero"

    TAG1=$(get_latest_version_github "$REPO1")
    TAG2=$(get_latest_version_github "$REPO2")

    FILTER1="$ubuntu_version"
  
    # Get .deb package URLs for each repository
    DEB_URLS1=$(get_deb_urls_no_api $REPO1 $TAG1 $FILTER1)
    DEB_URLS2=$(get_deb_urls_no_api $REPO2 $TAG2 $FILTER1)

    # Merge the results into a single array
    package_urls=()
    while IFS= read -r url; do
        package_urls+=("$url")
    done <<< "$DEB_URLS1"

    while IFS= read -r url; do
        package_urls+=("$url")
    done <<< "$DEB_URLS2"

    # Iterate over the list of package URLs and download each one
    for package_url in "${package_urls[@]}"; do
        download_deb_package "$package_url" "$APT_GET_TIMEOUT"
        install_deb_package "$package_url" "$APT_GET_TIMEOUT"
    done
    
    
    # Return to the original directory
    popd > /dev/null || handle_error "Failed to return to original directory"
}


# ***********************************************************************
need_to_reboot=0
need_to_logout=0

# Detect Ubuntu version
ubuntu_version=$(lsb_release -rs)

# Get the CPU family and model information
cpu_family=$(grep -m 1 'cpu family' /proc/cpuinfo | awk '{print $4}')
cpu_model=$(grep -m 1 'model' /proc/cpuinfo | awk '{print $3}')
cpu_model_number=$((cpu_model))
cpu_model_name=$(lscpu | grep "Model name:" | awk -F: '{print $2}' | xargs)

echo_color "\n CPU is Intel Family $cpu_family Model $cpu_model ($cpu_model_name).\n" "yellow"

# Choose the package list based on the Ubuntu version
case "$ubuntu_version" in
    24.04)
        echo_color " Detected Ubuntu version: $ubuntu_version. " "green"
        INTEL_CL_GPU_REPO_URL=$INTEL_CL_GPU_REPO_URL_24
        INTEL_DC_GPU_REPO_URL=$INTEL_DC_GPU_REPO_URL_24
        ;;
    22.04)
        echo_color " Detected Ubuntu version: $ubuntu_version. " "green"
        INTEL_CL_GPU_REPO_URL=$INTEL_CL_GPU_REPO_URL_22
        INTEL_DC_GPU_REPO_URL=$INTEL_DC_GPU_REPO_URL_22
        ;;
    *)
        echo_color " Unsupported Ubuntu version: $ubuntu_version. Exiting." "red"
        exit 1
        ;;
esac


# Check if the CPU is Intel Family 6 Model 189 (Lunar Lake)
if [[ "$cpu_family" == "6" && "$cpu_model" == "189" ]]; then

    check_kernel_version
    status=$?

    if [ $status -eq 0 ]; then
        echo_color " Kernel 6.12 or higher detected." "green"
    else
        echo_color "\n WARNING!" "red"
        echo_color "\n Intel® Deep Learning Streamer on Lunar Lake family processors has only been tested with the 6.12 kernel. We strongly recommend updating the kernel version before proceeding." "red"
        read -p " Quit installation? [y/n] " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            exit
        fi
    fi
fi

#-----------------------STEP 1-------------------------------------------

# Inform the user about the upcoming package installation
echo_color "\n The script will now update package lists and install required packages." "yellow"

update_package_lists
install_packages curl wget jq gpg software-properties-common pciutils

#-----------------------INSTALL GPU DRIVERS------------------------------

# Initialize the GPU state variable
# 0 - No Intel® GPU
# 1 - Intel® client GPU
# 2 - Intel® Data Center GPU
intel_gpu_state=0
intel_gpu_driver_state=0
gpu_info=""

# Check for any Intel® GPU
intel_gpus=$(lspci -nn | grep -E 'VGA|3D|Display' | grep -i "Intel")

# If Intel® GPUs are found, prioritize DC GPU
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
        echo_color "\n Intel® Data Center GPU detected:" "bgreen"
        echo -e " -----------------------------------"
        echo -e " $gpu_info"
        echo -e " -----------------------------------"
        ;;
esac


if [ $intel_gpu_state -ne 0 ]; then

    configure_repository "$INTEL_ONEAPI_KEY_URL" "$INTEL_ONEAPI_KEYRING_PATH" "$INTEL_ONEAPI_REPO_URL" "$INTEL_ONEAPI_LIST"
    echo_color "\n Intel® One API repository has been configured.\n" "green"
    update_package_lists

    setup_gpu

    if [[ "$ubuntu_version" == "24.04" ]]; then
        echo " Updating GPU setup..."

        repo="intel/compute-runtime"  # Replace with the GitHub repository in the format "owner/repo"
        package_name="intel-opencl-icd"
        
        latest_version=$(get_latest_version_github "$repo")
        latest_version="${latest_version#v}" # Remove the leading "v" if present
        installed_version=$(get_installed_version "$package_name")
        #installed_version=$(echo "$installed_version" | grep -oP '^\d+\.\d+\.\d+') # This extracts the version number in the format X.Y.Z 

        if [ -z "$installed_version" ]; then
            echo "The package '$package_name' is not installed."
            update_gpu_setup
        else
            echo "Latest version of '$package_name' from GitHub: $latest_version"
            echo "Installed version of '$package_name': $installed_version"

            if [ "$latest_version" == "$installed_version" ]; then
                echo "Latest version installed"
            else
                update_gpu_setup    # look for the latest drivers in intel/intel-graphics-compiler and intel/compute-runtime repositories
            fi
        fi
    fi

    #-----------CHECK IF GPU DRIVERS ARE INSTALLED-------------------------------

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

    if [ -n "$render_devices" ]; then
        echo_color "\n GPU drivers have been installed.\n" "green"
        intel_gpu_driver_state=1
    fi
fi

#----------------------ADD USERS TO GROUPS---------------------------------
echo_color " Adding user to video and render groups..." "yellow"

add_user_to_group_if_not_member "render"
if [ $? -ne 0 ]; then
  need_to_logout=1
fi

add_user_to_group_if_not_member "video"
if [ $? -ne 0 ]; then
  need_to_logout=1
fi
#sudo usermod -a -G video "$USER" || handle_error "Failed to add user to video group."
#sudo usermod -a -G render "$USER" || handle_error "Failed to add user to render group."

#----------------------INSTALL MEDIA DRIVER--------------------------------
update_package_lists
install_packages intel-media-va-driver-non-free 

#-------------------------STEP 3-------------------------------------------

if [[ "$cpu_model" != *"Xeon"* ]] && (( cpu_model_number >= 170 )); then
    # In this case we know that NPU must be present in the system, so we can proceed with the installation
    echo_color " This system contains a Neural Processing Unit." "green"

    repo="intel/linux-npu-driver"  # Replace with the GitHub repository in the format "owner/repo"
    package_name="intel-driver-compiler-npu"
    
    latest_version=$(get_latest_version_github "$repo")
    latest_version="${latest_version#v}" # Remove the leading "v" if present
    installed_version=$(get_installed_version "$package_name")
    installed_version=$(echo "$installed_version" | grep -oP '^\d+\.\d+\.\d+') # This extracts the version number in the format X.Y.Z 

    if [ -z "$installed_version" ]; then
        echo "The package '$package_name' is not installed."
        setup_npu
        need_to_reboot=1
    else
        echo "Latest version of '$package_name' from GitHub: $latest_version"
        echo "Installed version of '$package_name': $installed_version"

        if [ "$latest_version" == "$installed_version" ]; then
            echo "The installed version is up-to-date."

            intel_npu=$(lspci | grep -i 'Intel' | grep 'NPU' | rev | cut -d':' -f1 | rev)

            if [ -z "$intel_npu" ]; then
                intel_npu="Intel® NPU"
            fi

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
            echo "The installed version is not up-to-date."
            setup_npu
            need_to_reboot=1
        fi
    fi
fi

if [ "$need_to_reboot" -eq 1 ]; then
  echo_color "\n A reboot is required!." "bred"

  # Ask the user for permission to reboot
  read -p " Do you want to reboot now? (y/n): " answer

  # Check the user's response
  if [[ "$answer" =~ ^[Yy]$ ]]; then
    echo_color " Remember to rerun the script after the reboot." "cyan"
    echo " Rebooting the system..."
    sudo reboot
  else
    echo " Reboot canceled."
    exit
  fi
else 
    echo_color "\n Environment setup completed successfully. " "bgreen"
    echo_color " You may now proceed with the installation of Intel® DL Streamer." "green"

    echo " ---------------------------------------------------"
    echo  " The following hardware will be enabled: "
    echo  " - CPU ($cpu_model_name) "

    if [ $intel_gpu_driver_state -ne 0 ]; then
        short_gpu_info=$(echo "$gpu_info" | grep -o "Intel.*")
        echo " - GPU ($short_gpu_info)"
    fi

    if [ -n "$intel_npu" ]; then
        echo " - NPU ($intel_npu)"
    fi

    echo " ---------------------------------------------------"

    if [ -z "$render_devices" ]; then
        echo_color "\n Intel® GPU hardware is present but drivers could not be installed." "yellow"

        case $intel_gpu_state in
        1)
            echo_color " To enable GPU support, install the client GPU drivers manually by following the instructions available at https://dgpu-docs.intel.com/driver/client/overview.html#installing-gpu-packages.\n" "cyan"
            ;;
        2)
            echo " To enable GPU support, install Intel® Data Center GPU drivers manually by following the instructions available at https://dgpu-docs.intel.com/driver/installation.html.\n" "cyan"
            ;;
        esac

    fi

    if [ "$need_to_logout" -eq 1 ]; then
        echo_color "User added to render and video groups. Please log out and log back in for the changes to take effect." "cyan"
    fi
fi


