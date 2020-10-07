/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef __C_METAPUBLISH_KAFKA_H__
#define __C_METAPUBLISH_KAFKA_H__
#ifdef KAFKA_INC

#include "i_metapublish_method.h"
#include "librdkafka/rdkafka.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define METAPUBLISH_TYPE_KAFKA metapublish_kafka_get_type()
G_DECLARE_FINAL_TYPE(MetapublishKafka, metapublish_kafka, METAPUBLISH, KAFKA, GObject)

G_END_DECLS

#endif /* KAFKA_INC */

#endif /* __C_METAPUBLISH_KAFKA_H__ */
