/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "video_inference.h"

#include <string>

G_BEGIN_DECLS

#define OBJECT_TRACK_NAME "Object tracking"
#define OBJECT_TRACK_DESCRIPTION "Assigns unique ID to detected objects"

GType object_track_get_type(void);

G_END_DECLS
