/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gva_base_inference.h"
#include <gst/gst.h>
#include <map>
#include <string>

std::map<std::string, GstStructure *> ReadModelProc(std::string filepath);

gboolean is_preprocessor(const GstStructure *processor);
