/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gvametapublishbase.hpp>

G_BEGIN_DECLS

#define GST_TYPE_GVA_META_PUBLISH_MQTT (gva_meta_publish_mqtt_get_type())
#define GVA_META_PUBLISH_MQTT(obj)                                                                                     \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_META_PUBLISH_MQTT, GvaMetaPublishMqtt))
#define GVA_META_PUBLISH_MQTT_CLASS(klass)                                                                             \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_META_PUBLISH_MQTT, GvaMetaPublishMqttClass))
#define IS_GVA_META_PUBLISH_MQTT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_META_PUBLISH_MQTT))
#define IS_GVA_META_PUBLISH_MQTT_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_META_PUBLISH_MQTT))
#define GVA_META_PUBLISH_MQTT_GET_CLASS(obj)                                                                           \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_GVA_META_PUBLISH_MQTT, GvaMetaPublishMqttClass))

struct GvaMetaPublishMqtt {
    GvaMetaPublishBase base;

    class GvaMetaPublishMqttPrivate *impl;
};

struct GvaMetaPublishMqttClass {
    GvaMetaPublishBaseClass base;
};

GType gva_meta_publish_mqtt_get_type(void);

G_END_DECLS
