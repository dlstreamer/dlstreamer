#!/bin/bash
# ==============================================================================
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

# Define the maximum time allowed for web operations
WEB_TIMEOUT=600
APT_UPDATE_TIMEOUT=30
APT_GET_TIMEOUT=600
RELEASES_URL="https://github.com/dlstreamer/dlstreamer/releases/latest"
DLS_VERSION=""

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

# ***********************************************************************

# Function to add permissions for the Docker container to connect to the X server
add_xserver_permissions() {
    # Allow local connections from the root user
    if ! xhost local:root; then
        return 1 # Return with an error status
    fi

    # Set access control list for the .Xauthority file for user with UID 1000
    if ! setfacl -m user:1000:r ~/.Xauthority; then
        return 1 # Return with an error status
    fi

    return 0 # Return with success status
}

# ***********************************************************************

# Verify that you can run docker commands without sudo
echo_color " Verifying that you can run docker commands without sudo..." "yellow"
if ! docker run hello-world; then
    echo_color " Unable to run 'docker run hello-world'." "red"
    echo_color " This could be due to network issues, Docker Hub being down, or other connectivity problems." "yellow"
    echo_color " Please check your network connection and try again." "yellow"
    exit 1
fi


update_package_lists
install_packages acl x11-xserver-utils

# Check for the common error when previously using sudo
DOCKER_CONFIG="$HOME/.docker"
if [ -d "$DOCKER_CONFIG" ]; then
    if [ ! "$(stat -c '%U' $DOCKER_CONFIG)" = "$USER" ]; then
        echo_color " Fixing permissions for the $DOCKER_CONFIG directory..." "yellow"
        if ! sudo chown "$USER":"$USER" "$DOCKER_CONFIG" -R; then
            echo_color " Failed to change ownership of $DOCKER_CONFIG to $USER." "red"
            exit 1
        fi
        if ! sudo chmod g+rwx "$DOCKER_CONFIG" -R; then
            echo_color " Failed to change permissions for $DOCKER_CONFIG." "red"
            exit 1
        fi
        echo_color " Permissions for $DOCKER_CONFIG have been fixed." "green"
    fi
else
    echo_color " The $DOCKER_CONFIG directory does not exist. " "yellow"
fi


# Main script execution
echo_color " Configuring permissions for Docker container to connect to X server..." "yellow"

# Call the function to add permissions
if add_xserver_permissions; then
    echo_color " Permissions configured successfully. Docker container should be able to connect to X server." "green"
else
    # If the permission configuration fails, inform the user
    echo_color " Connection from Docker container to X server on host will not be possible." "red"
fi


latest_url=$(curl -Ls $RELEASES_URL | grep -oE 'include-fragment[^>]+src="([^"]+)"' | grep -oE 'https://github.com/dlstreamer/dlstreamer[^"]+')
DLS_VERSION=$(basename "$latest_url")
DLS_VERSION="${DLS_VERSION:1}"

ubuntu_version=$(lsb_release -rs)

# Choose the package list based on the Ubuntu version
case "$ubuntu_version" in
    24.04)
        DLS_VERSION+="-ubuntu24"
        ;;
    22.04)
        DLS_VERSION+="-ubuntu22"
        ;;
    *)
        echo_color " Unsupported Ubuntu version: $ubuntu_version. Exiting." "red"
        exit 1
        ;;
esac

docker_image="intel/dlstreamer:$DLS_VERSION"

if timeout --foreground $WEB_TIMEOUT docker pull $docker_image; then
    echo_color " Docker image $docker_image pulled successfully." "green"
else
    exit_status=$?
    if [ $exit_status -eq 124 ]; then
        handle_error " Docker pull command timed out." 
    else
        handle_error " Failed to pull Docker $docker_image image due to an error."
    fi
fi

# Start a Docker container in detached mode
container_id=$(docker run -d --rm $docker_image tail -f /dev/null)

# Check if the container started successfully
if [ -z "$container_id" ]; then
    handle_error " Failed to start the Docker container."
fi

echo_color  " Docker container started with ID: $container_id" "green"

container_name=$(docker ps --filter "id=$container_id" --format "{{.Names}}")
echo " The name of the container is: $container_name"

# Execute a command inside the running container

if ! docker exec -it "$container_name" gst-inspect-1.0 gvadetect; then
    echo_color " Failed to execute the command inside the Docker container." "red"
    # Optionally stop the container if the command fails
    docker stop "$container_id"
    exit 1
fi

echo_color  " Intel® DL Streamer the Docker container has been set up successfully." "bgreen"

# Stop the container after the command execution
echo 
read -p " Intel® DL Streamer Docker container is running in a detached mode. Do you want to stop it? [y/N] " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo_color " Wait while the container is being stopped..." "yellow"
    echo
    docker stop "$container_name"
    echo
    echo_color " You can start Intel® DL Streamer Docker container later with the following command:" "cyan"
    echo -e "\t docker run -it --rm $docker_image" 
    echo
else
    echo
    echo_color " You can now execute commands inside running Intel® DL Streamer Docker container, for example:" "cyan"
    echo -e "\t docker exec -it $container_name gst-inspect-1.0 gvadetect"
    echo 
    echo_color " To stop current Intel® DL Streamer Docker container, use the following command:" "cyan"
    echo -e "\t docker stop $container_name"
    echo
fi

