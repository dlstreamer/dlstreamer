/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __CONVERTERS_H__
#define __CONVERTERS_H__

#include <gst/gst.h>
#include <string.h>

#include "gstgvametaconvert.h"
#include "jsonconverter.h"

#ifdef __cplusplus
extern "C" {
#endif

GHashTable *get_converters();

#ifdef __cplusplus
}
#endif

#endif /* __CONVERTERS_H__ */
