/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_storage.hpp"

InferenceInstances::map_type InferenceInstances::_instances;
std::mutex InferenceInstances::_mutex;
