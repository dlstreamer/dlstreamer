# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import os
import sys
MODULE_DIR_PATH = os.path.dirname(__file__)
sys.path.insert(0, MODULE_DIR_PATH)
REG_DIR_PATH = os.path.join(os.path.dirname(MODULE_DIR_PATH), "")
sys.path.append(REG_DIR_PATH)
