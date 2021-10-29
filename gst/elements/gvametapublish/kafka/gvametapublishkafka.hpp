/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gvametapublishbase.hpp>

G_BEGIN_DECLS

#define GST_TYPE_GVA_META_PUBLISH_KAFKA (gva_meta_publish_kafka_get_type())
#define GVA_META_PUBLISH_KAFKA(obj)                                                                                    \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_META_PUBLISH_KAFKA, GvaMetaPublishKafka))
#define GVA_META_PUBLISH_KAFKA_CLASS(klass)                                                                            \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_META_PUBLISH_KAFKA, GvaMetaPublishKafkaClass))
#define IS_GVA_META_PUBLISH_KAFKA(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_META_PUBLISH_KAFKA))
#define IS_GVA_META_PUBLISH_KAFKA_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_META_PUBLISH_KAFKA))
#define GVA_META_PUBLISH_KAFKA_GET_CLASS(obj)                                                                          \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_GVA_META_PUBLISH_KAFKA, GvaMetaPublishKafkaClass))

struct GvaMetaPublishKafka {
    GvaMetaPublishBase base;
    class GvaMetaPublishKafkaPrivate *impl;
};

struct GvaMetaPublishKafkaClass {
    GvaMetaPublishBaseClass base;
};

GType gva_meta_publish_kafka_get_type(void);

G_END_DECLS
