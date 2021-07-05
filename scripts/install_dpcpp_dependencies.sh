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
REQUIRED_DRIVER_VERSION="21.19.19792"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
UPGRADE_DRIVER=OFF
UNINSTALL_DRIVER=OFF
DPCPP_VERSION="2021.2.0"

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
    if [[ $DISTRO == ubuntu ]]; then
        UBUNTU_VERSION=$(grep -m1 'VERSION_ID' /etc/os-release | grep -Eo "[0-9]{2}.[0-9]{2}")
        if [[ $UBUNTU_VERSION != '18.04' && $UBUNTU_VERSION != '20.04' ]]; then
            echo
            echo "Error: This package can be installed only on Ubuntu 18.04 or Ubuntu 20.04."
            echo "More info https://software.intel.com/content/www/us/en/develop/articles/intel-oneapi-dpcpp-system-requirements" >&2
            echo "Installation of Intel® oneAPI DPC++ Compiler interrupted"
            exit $EXIT_FAILURE
        fi
    else
        echo
        echo "Error: Installation of this package is supported only on Ubuntu 18.04 or Ubuntu 20.04."
        echo "Installation of Intel® oneAPI DPC++ Compiler interrupted"
        exit $EXIT_FAILURE
    fi
}

distro_init()
{
    if [[ -f /etc/lsb-release ]]; then
        DISTRO="ubuntu"
    else
        echo "Unsupported OS ${DISTRO}"
        exit $EXIT_WRONG_ARG
    fi

    _check_distro_version
}

verify_checksum()
{
    curl -L -O "https://github.com/intel/compute-runtime/releases/download/$REQUIRED_DRIVER_VERSION/ww19.sum"
    sha256sum -c ww19.sum
}

uninstall_user_mode()
{
    echo "Looking for previously installed user-mode driver..."

    PACKAGES=("intel-gmmlib"
              "intel-igc-core"
              "intel-igc-opencl"
              "intel-opencl"
              "intel-ocloc"
              "intel-level-zero-gpu")

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

download_packages()
{
    _install_packages "curl"

    mkdir -p "$SCRIPT_DIR/neo"
    cd "$SCRIPT_DIR/neo" || exit

    curl -L -O https://github.com/intel/compute-runtime/releases/download/21.19.19792/intel-gmmlib_21.1.2_amd64.deb
    curl -L -O https://github.com/intel/intel-graphics-compiler/releases/download/igc-1.0.7181/intel-igc-core_1.0.7181_amd64.deb
    curl -L -O https://github.com/intel/intel-graphics-compiler/releases/download/igc-1.0.7181/intel-igc-opencl_1.0.7181_amd64.deb
    curl -L -O https://github.com/intel/compute-runtime/releases/download/21.19.19792/intel-opencl_21.19.19792_amd64.deb
    curl -L -O https://github.com/intel/compute-runtime/releases/download/21.19.19792/intel-ocloc_21.19.19792_amd64.deb
    curl -L -O https://github.com/intel/compute-runtime/releases/download/21.19.19792/intel-level-zero-gpu_1.1.19792_amd64.deb

    verify_checksum
    if [[ $? -ne 0 ]]; then
        echo "ERROR: checksums do not match for the downloaded packages"
        echo "       Please check your Internet connection and make sure you have enough disk space or fix the problem manually and try again."
        exit $EXIT_FAILURE
    fi
}

_deploy_deb()
{
    cmd="dpkg -i $1"
    echo "$cmd"
    eval "$cmd"
}

install_user_mode()
{
    echo "Installing user mode driver..."

    _deploy_deb "intel*.deb"
    if [[ $? -ne 0 ]]; then
        echo "ERROR: failed to install rpms $cmd error"  >&2
        echo "Make sure you have enough disk space or fix the problem manually and try again." >&2
        exit $EXIT_FAILURE
    fi

    # exit from $SCRIPT_DIR/neo folder
    cd - || exit

    # clean it up
    rm -rf "$SCRIPT_DIR/neo"
}

add_user_to_video_group()
{
    local real_user
    real_user=$(logname 2>/dev/null || echo "${SUDO_USER:-${USER}}")
    echo
    echo "Adding $real_user to the video group..."
    usermod -a -G video "$real_user"
    if [[ $? -ne 0 ]]; then
        echo "WARNING: unable to add $real_user to the video group" >&2
    fi
    echo "Adding $real_user to the render group..."
    usermod -a -G render "$real_user"
    if [[ $? -ne 0 ]]; then
        echo "WARNING: unable to add $real_user to the render group" >&2
    fi
}

install_driver()
{
    uninstall_user_mode
    download_packages
    install_user_mode
    add_user_to_video_group

    echo
    echo "Installation of Intel® GPU driver $REQUIRED_DRIVER_VERSION completed successfully."
    echo
    echo "Next steps:"
    echo "Add OpenCL users to the video and render group: 'sudo usermod -a -G video,render USERNAME'"
    echo "   e.g. if the user running OpenCL host applications is foo, run: sudo usermod -a -G video,render foo"
    echo "   Current user has been already added to the video and render group"
    echo
}

check_specific_generation()
{
    echo "Checking processor generation..."
    specific_generation=$(grep -m1 'model name' /proc/cpuinfo | grep -E "i[357]-1[01][0-9]{2,4}N?G[147R]E?")
    if [[ -z "$specific_generation" && "$UPGRADE_DRIVER" != 'ON' ]]; then
        echo
        echo "Warning: Intel® oneAPI DPC++ Compiler needs Intel® GPU driver $REQUIRED_DRIVER_VERSION or higher."
        echo "Warning: Your generation Intel® Core™ processor is older than 10th generation Intel® Core™ processor (formerly Ice Lake) or 11th generation Intel® Core™ processor (formerly Tiger Lake)."
        echo "The newest version of Intel® GPU driver may cause performance degradation of inference in your platform."
        echo "If you agree and want to continue, please run script again with --upgrade_driver parameter. In this case Intel® GPU driver will be upgraded automatically."
        exit
    fi
}

check_current_driver()
{
    gfx_version=$(apt show intel-opencl | grep Version)
    gfx_version="$(echo -e "${gfx_version}" | sed -e 's/^Version[[:space:]]*\:[[:space:]]*//')"
    if [[ -z $gfx_version || $gfx_version < "$REQUIRED_DRIVER_VERSION" ]]; then
        echo
        echo "Warning: Intel® GPU driver $REQUIRED_DRIVER_VERSION will be installed."
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
    if [[ $INSTALL_PACKAGE_TYPE == 'runtime' ]]; then
        PREREQUISITES="wget gpg-agent software-properties-common"
        LEVEL_ZERO_PACKAGES="level-zero intel-level-zero-gpu"
        DPCPP_PACKAGE="intel-oneapi-compiler-dpcpp-cpp-runtime-$DPCPP_VERSION"
    elif [[ $INSTALL_PACKAGE_TYPE == 'devel' ]]; then
        PREREQUISITES="wget gpg-agent software-properties-common ocl-icd-opencl-dev opencl-headers"

        if [[ $UBUNTU_VERSION == '18.04' ]]; then
            LEVEL_ZERO_PACKAGES="level-zero-devel intel-level-zero-gpu"
        elif [[ $UBUNTU_VERSION == '20.04' ]]; then
            LEVEL_ZERO_PACKAGES="level-zero-dev intel-level-zero-gpu"
        fi

        DPCPP_PACKAGE="intel-oneapi-compiler-dpcpp-cpp-$DPCPP_VERSION"
    fi

    if [[ $UBUNTU_VERSION == '18.04' ]]; then
        GRAPHICS_REPOSITORY="deb [arch=amd64] https://repositories.intel.com/graphics/ubuntu bionic main"
    elif [[ $UBUNTU_VERSION == '20.04' ]]; then
        GRAPHICS_REPOSITORY="deb [arch=amd64] https://repositories.intel.com/graphics/ubuntu focal main"
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
    choose_proper_packages
    install_prerequisites
    install_level_zero
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
        check_specific_generation
        check_current_driver
        install
        install_summary
    fi
}

[[ "$0" == "${BASH_SOURCE[0]}" ]] && main "$@"
