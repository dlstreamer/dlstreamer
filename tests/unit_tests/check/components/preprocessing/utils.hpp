/*******************************************************************************
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>
#include <string>
#include <vector>

GValueArray *ConvertVectorToGValueArr(const std::vector<double> &vector);
void compareArrays(const std::vector<double> &first, const std::vector<double> &second);

void checkErrorThrowWithInvalidGstStructure(const std::string &field_name,
                                            const std::vector<double> invalid_arr = std::vector<double>());
