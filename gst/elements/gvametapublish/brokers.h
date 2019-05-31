/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __BROKERS_H__
#define __BROKERS_H__

#include "filepublisher.h"
#include "gstgvametapublish.h"
#ifdef KAFKA_INC
#include "kafkapublisher.h"
#endif
#ifdef PAHO_INC
#include "mqttpublisher.h"
#endif
#include <gst/gst.h>
#include <string.h>

typedef struct {
    const gchar *name;
    broker_initfunction_type initializefunction;
    broker_function_type function;
    broker_finalizefunction_type finalizefunction;
} BrokerMap;

extern BrokerMap brokers[];
extern const int lengthOfBrokers;

#endif /* __BROKERS_H__ */
