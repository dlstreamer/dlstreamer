#!/bin/bash
# ==============================================================================
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

EXIT_FAILURE=1
EXIT_WRONG_ARG=2
INSTALL_PACKAGE_TYPE=
AVAILABLE_TYPES=("devel" "runtime")
REQUIRED_DRIVER_VERSION="21.29.20389"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
UPGRADE_DRIVER=OFF
UNINSTALL_DRIVER=OFF
DPCPP_VERSION="2021.2.0"

GFX_VERSION=""
GRAPHICS_REPOSITORY="deb [arch=amd64] https://repositories.intel.com/graphics/ubuntu focal main"

print_help()
{
    # Display Help
    usage="Usage: $(basename "$0") [OPTIONS]...
Download and install Intel® oneAPI DPC++ Compiler

    Available options:
    --type                  Choose a type of DPC++ package installation.
                            Available types: devel, runtime.
    --upgrade_driver        Enable automatically upgrading of Intel® GPU driver.
    --uninstall_driver      Uninstall OpenCL user-mode driver and exit.
    -h, --help              Display this help and exit"
    echo "$usage"
}

while [[ $# -gt 0 ]]
do
    key="$1"
    case $key in
        --type)
        user_chosen_type="$2"
        if [[ " ${AVAILABLE_TYPES[*]} " =~ " ${user_chosen_type} " ]]; then
            INSTALL_PACKAGE_TYPE=$user_chosen_type
        else
            echo "ERROR: the type '${user_chosen_type}' is inappropriate."
            echo "Available values: ${AVAILABLE_TYPES[*]}"
            exit $EXIT_WRONG_ARG
        fi
        shift
        shift
    ;;
        --upgrade_driver)
        UPGRADE_DRIVER=ON
        shift
    ;;
        --uninstall_driver)
        UNINSTALL_DRIVER=ON
        shift
    ;;
        -h|--help)
        print_help
        exit
    ;;
        *)
        echo "$(basename "$0"): invalid option -- '${key}'"
        echo "Try '$(basename "$0") --help' for more information."
        exit $EXIT_WRONG_ARG
    esac
done

if [[ "$UNINSTALL_DRIVER" != "ON" ]] && [[ -z "$INSTALL_PACKAGE_TYPE" ]]; then
    echo "ERROR: you should specify --type option."
    echo "Try '$(basename "$0") --help' for more information."
    exit $EXIT_WRONG_ARG
fi

_install_packages()
{
    PACKAGES=$1
    CMDS=("apt-get -y update"
          "DEBIAN_FRONTEND='noninteractive' apt-get -y install --no-install-recommends ${PACKAGES}")

    for cmd in "${CMDS[@]}"; do
        echo "$cmd"
        eval "$cmd"
        if [[ $? -ne 0 ]]; then
            echo "ERROR: failed to run $cmd" >&2
            echo "Problem (or disk space)?" >&2
            echo "                sudo -E $0" >&2
            echo "2. Verify that you have enough disk space, and run the script again." >&2
            exit $EXIT_FAILURE
        fi
    done
}

_check_distro_version()
{
    UBUNTU_VERSION=$(grep -m1 'VERSION_ID' /etc/os-release | grep -Eo "[0-9]{2}.[0-9]{2}")
    if [[ $UBUNTU_VERSION != '20.04' ]]; then
        echo
        echo "Error: Installation of this package is supported only on Ubuntu 20.04."
        echo "Installation of Intel® oneAPI DPC++ Compiler interrupted"
        exit $EXIT_FAILURE
    fi
}

distro_init()
{
    if [[ -f /etc/lsb-release ]]; then
        DISTRO="ubuntu"
        _check_distro_version
    else
        echo "Unsupported OS ${DISTRO}"
        exit $EXIT_WRONG_ARG
    fi
}

uninstall_user_mode()
{
    add-apt-repository --remove "$GRAPHICS_REPOSITORY"

    echo "Looking for previously installed user-mode driver..."

    PACKAGES=("intel-opencl"
              "intel-opencl-icd"
              "intel-ocloc"
              "intel-gmmlib"
              "intel-igc-core"
              "intel-igc-opencl")

    for package in "${PACKAGES[@]}"; do
        found_package=$(dpkg-query -W -f='${binary:Package}\n' "${package}")
        if [[ $? -eq 0 ]]; then
            cmd="apt-get autoremove -y $found_package"
            echo "$cmd"
            eval "$cmd"
            if [[ $? -ne 0 ]]; then
                echo "ERROR: failed to uninstall existing user-mode driver." >&2
                echo "Please try again manually and run the script again." >&2
                exit $EXIT_FAILURE
            fi
        fi
    done
}

uninstall_summary()
{
    echo
    echo "OpenCL user-mode driver has been uninstalled successfully."
    echo
    echo "Next steps:"
    echo "Plese run install_NEO_OCL_driver.sh script from OpenVINO install directory again to restore original OCL driver."
    echo
    echo "Note:"
    echo "DPC++ and Level Zero have not been removed."
    echo
}

install_driver()
{   
    add-apt-repository --remove "$GRAPHICS_REPOSITORY"

    openvino_dir="/opt/intel/openvino_2021"
    sudo -E ${openvino_dir}/install_dependencies/install_NEO_OCL_driver.sh -d ${REQUIRED_DRIVER_VERSION} -y
    if [[ $? -ne 0 ]]; then
        echo
        echo "Error occurred while installing Intel® GPU driver via install_NEO_OCL_driver.sh"
        echo "Plese try to run install_NEO_OCL_driver.sh script manually from OpenVINO directory to install required Intel® GPU driver."
        exit $EXIT_FAILURE
    fi
}

check_current_driver()
{
    if dpkg -s intel-opencl > /dev/null 2>&1; then
        GFX_VERSION=$(dpkg -s intel-opencl | grep Version)
    elif dpkg -s intel-opencl-icd > /dev/null 2>&1; then
        GFX_VERSION=$(dpkg -s intel-opencl-icd | grep Version)
    fi

    GFX_VERSION="$(echo -e "${GFX_VERSION}" | grep -Eo "[0-9]{2,3}\.[0-9]{2,3}\.[0-9]{3,6}")"
}

check_agreement()
{
    if [ "$UPGRADE_DRIVER" == "ON" ]; then
        return 0
    fi

    echo
    echo "OpenCL™ Driver $REQUIRED_DRIVER_VERSION is required to be installed."
    echo "But another one is installed in your system."
    echo "It's necessary to upgrade OpenCL™ Driver up to $REQUIRED_DRIVER_VERSION to continue installation."
    echo "Otherwise, this installation will be interrupted."
    while true; do
        read -p "Are you agree to upgrade OpenCL™ Driver up to $REQUIRED_DRIVER_VERSION? (y/n): " yn
        case $yn in
            [Yy]*) return 0  ;;
            [Nn]*) exit $EXIT_FAILURE ;;
        esac
    done
}

check_installation_possibility()
{
    check_current_driver

    # install NEO OCL driver if the current driver version < REQUIRED_DRIVER_VERSION
    if [[ -z $GFX_VERSION || ! "$(printf '%s\n' "$REQUIRED_DRIVER_VERSION" "$GFX_VERSION" | sort -V | head -n 1)" = "$REQUIRED_DRIVER_VERSION" ]]; then
        check_agreement
        install_driver
    fi
}

check_root_access()
{
    if [[ $EUID -ne 0 ]]; then
        echo
        echo "ERROR: you must run this script as root." >&2
        echo "Please try again with \"sudo -E $0\", or as root." >&2
        exit $EXIT_FAILURE
    fi
}

choose_proper_packages()
{
    PREREQUISITES="sudo wget gpg-agent software-properties-common"
    if [[ $INSTALL_PACKAGE_TYPE == 'runtime' ]]; then
        DPCPP_PACKAGE="intel-oneapi-compiler-dpcpp-cpp-runtime-$DPCPP_VERSION"
    elif [[ $INSTALL_PACKAGE_TYPE == 'devel' ]]; then
        LEVEL_ZERO_PACKAGES="level-zero-dev"
        DPCPP_PACKAGE="intel-oneapi-compiler-dpcpp-cpp-$DPCPP_VERSION"
    fi
}

_install_prerequisites_ubuntu()
{
    _install_packages "${PREREQUISITES}"
}

install_prerequisites()
{
    echo "Installing prerequisites..."
    if [[ $DISTRO == "ubuntu" ]]; then
        _install_prerequisites_ubuntu
    else
        echo "Unknown OS"
    fi
}

install_level_zero()
{
    wget -qO - https://repositories.intel.com/graphics/intel-graphics.key | apt-key add -
    apt-add-repository "${GRAPHICS_REPOSITORY}"

    _install_packages "${LEVEL_ZERO_PACKAGES}"
}

install_dpcpp()
{
    curl -fsSL https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | apt-key add - 
    apt-add-repository "deb https://apt.repos.intel.com/oneapi all main"

    _install_packages "${DPCPP_PACKAGE}"
}

install()
{
    if [[ $INSTALL_PACKAGE_TYPE == 'devel' ]]; then
        install_level_zero
    fi
    install_dpcpp
}

install_summary()
{
    echo
    echo "Installation completed successfully."
    echo
    echo "Next steps:"
    echo "Please enable Intel® oneAPI DPC++ Compiler development environment by running the following command:"
    echo "source /opt/intel/oneapi/compiler/$DPCPP_VERSION/env/vars.sh"
    echo
}

main()
{
    echo "Intel® oneAPI DPC++ Compiler installer"
    distro_init
    check_root_access
    if [[ "$UNINSTALL_DRIVER" == "ON" ]]; then
        uninstall_user_mode
        uninstall_summary
    else
        choose_proper_packages
        install_prerequisites
        check_installation_possibility
        install
        install_summary
    fi
}

[[ "$0" == "${BASH_SOURCE[0]}" ]] && main "$@"
