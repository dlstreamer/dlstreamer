#!/bin/bash
# ==============================================================================
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
yes | ./DLS_install_prerequisites.sh

yes | ./DLS_install_deb_packages.sh -v 2024.1.2 -u "$1" -u "$2"