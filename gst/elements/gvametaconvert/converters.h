/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __CONVERTERS_H__
#define __CONVERTERS_H__

#include "gstgvametaconvert.h"
#include "jsonconverter.h"
#include <gst/gst.h>
#include <string.h>

typedef struct {
    const gchar *name;
    convert_function_type function;
} ConverterMap;

extern ConverterMap converters[];

#endif /* __CONVERTERS_H__ */
