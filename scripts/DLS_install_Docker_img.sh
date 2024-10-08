#!/bin/bash
# ==============================================================================
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

# Define the maximum time allowed for web operations
APT_UPDATE_TIMEOUT=30
APT_GET_TIMEOUT=600

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
# Function to create or update Docker config.json with proxy settings
configure_docker_proxy() {
    # Define the Docker config file path
    local docker_config_file="$HOME/.docker/config.json"

    # Read system proxy settings from environment variables
    local http_proxy="${HTTP_PROXY:-}"
    local https_proxy="${HTTPS_PROXY:-}"
    local ftp_proxy="${FTP_PROXY:-}"
    local no_proxy="${NO_PROXY:-}"

    # Define default no_proxy settings if not provided
    if [ -z "$no_proxy" ]; then
        no_proxy="localhost,127.0.0.0/1,10.0.0.0/1,172.16.0.0/20"
    fi

    # Create the .docker directory if it doesn't exist
    mkdir -p "$(dirname "$docker_config_file")"

    # Generate the proxy configuration JSON
    local proxy_json
    proxy_json=$(cat <<EOF
{
  "proxies": {
    "default": {
      "httpProxy": "$http_proxy",
      "httpsProxy": "$https_proxy",
      "noProxy": "$no_proxy",
      "ftpProxy": "$ftp_proxy"
    }
  }
}
EOF
)

    # Write the proxy configuration to the Docker config file
    if ! echo "$proxy_json" | jq . > "$docker_config_file"; then
        handle_error "Failed to write proxy settings to $docker_config_file"
    fi

    echo "Docker proxy configuration updated successfully."
    return 0
}

# Function to configure systemd proxy settings for Docker
configure_systemd_docker_proxy() {
    # Define the systemd Docker service override directory and file path
    local systemd_docker_dir="/etc/systemd/system/docker.service.d"
    local proxy_conf_file="${systemd_docker_dir}/proxy.conf"

    # Read system proxy settings from environment variables
    local http_proxy="${HTTP_PROXY:-}"
    local https_proxy="${HTTPS_PROXY:-}"
    local no_proxy="${NO_PROXY:-}"
    local ftp_proxy="${FTP_PROXY:-}"

    # Create the systemd Docker service override directory if it doesn't exist
    if ! sudo mkdir -p "$systemd_docker_dir"; then
        handle_error "Failed to create systemd Docker service override directory."
    fi

    # Write the proxy configuration to the proxy.conf file
    if ! sudo tee "$proxy_conf_file" > /dev/null <<EOF
[Service]
Environment="HTTP_PROXY=${http_proxy}"
Environment="HTTPS_PROXY=${https_proxy}"
Environment="NO_PROXY=${no_proxy}"
Environment="FTP_PROXY=${ftp_proxy}"
EOF
    then
        handle_error "Failed to write proxy settings to $proxy_conf_file"
    fi

    # Reload systemd daemon to apply changes
    if ! sudo systemctl daemon-reload; then
        handle_error "Failed to reload the systemd daemon."
    fi

    # Optionally restart Docker service to apply proxy settings
    echo_color " Systemd Docker proxy configuration updated successfully." "green"
    #echo "Please restart the Docker service manually to apply the changes."
    return 0
}


# Check if Docker is installed
if ! command -v docker &> /dev/null
then
    echo
    echo_color " Docker is not installed." "red"
    echo 
    read -p " Do you want to install Docker? [y/N] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo_color " Starting Docker installation process..." "yellow"
        update_package_lists
        install_packages ca-certificates curl jq
        sudo install -m 0755 -d /etc/apt/keyrings || handle_error "Could not create /etc/apt/keyrings directory."
        if ! sudo curl -fsSL --connect-timeout $APT_UPDATE_TIMEOUT https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc; then
            handle_error " Failed to download the GPG key. Exiting the script."
        fi
        sudo chmod a+r /etc/apt/keyrings/docker.asc || handle_error "Could not change /etc/apt/keyrings/docker.asc attributes."
        
        # Add the Docker repository to APT sources
        if ! timeout --foreground "$APT_UPDATE_TIMEOUT" bash -c 'echo \
        "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu \
        $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
        sudo tee /etc/apt/sources.list.d/docker.list > /dev/null'; then
            exit_status=$?
            if [ $exit_status -eq 124 ]; then
                handle_error "Adding the Docker repository to APT sources timed out after ${APT_UPDATE_TIMEOUT} seconds."
            else
                handle_error "Failed to add the Docker repository to APT sources."
            fi
        fi
        update_package_lists
        install_packages docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

    else
        handle_error "Docker installation is necessary to continue."   
    fi

    configure_docker_proxy
    configure_systemd_docker_proxy
    sudo systemctl restart docker
    
fi

echo_color "Docker is installed." "green"
docker --version

# Create the docker group and add your user
if ! getent group docker &> /dev/null; then
    echo "Creating docker group..." 
    sudo groupadd docker
fi

# Get the current username
CURRENT_USER=$(whoami)

echo "Adding user $CURRENT_USER to the docker group..." 
sudo usermod -aG docker $CURRENT_USER

# Inform the user that a new shell will be started
echo "Activating the changes to groups..." 

# Path to the second script
SECOND_SCRIPT="./_post_docker_setup.sh"

# Check if the second script exists and is executable
if [ ! -f "$SECOND_SCRIPT" ]; then
    echo_color "The script $SECOND_SCRIPT does not exist." "red"
    exit 1
elif [ ! -x "$SECOND_SCRIPT" ]; then
    echo_color "The script $SECOND_SCRIPT is not executable." "red"
    exit 1
fi

# Execute the second script with new group membership
exec sg docker -c "$SECOND_SCRIPT"