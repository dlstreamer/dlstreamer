/*******************************************************************************
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once
#include "gva_utils.h"
#include "tensor.h"
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>

nlohmann::json convert_tensor(const GVA::Tensor &s_tensor);