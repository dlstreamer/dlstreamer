/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvametapublishkafka.hpp"

#include <common.hpp>

GST_DEBUG_CATEGORY_STATIC(gva_meta_publish_kafka_debug_category);
#define GST_CAT_DEFAULT gva_meta_publish_kafka_debug_category

// Include after GST_CAT_DEFAULT define
#include "gvametapublishkafkaimpl.hpp"

class GvaMetaPublishKafkaPrivate : public GvaMetaPublishKafkaImpl<RdKafka::Producer, RdKafka::Topic> {
  public:
    GvaMetaPublishKafkaPrivate(GvaMetaPublishBase *base)
        : GvaMetaPublishKafkaImpl<RdKafka::Producer, RdKafka::Topic>(base) {
    }
};

G_DEFINE_TYPE_EXTENDED(GvaMetaPublishKafka, gva_meta_publish_kafka, GST_TYPE_GVA_META_PUBLISH_BASE, 0,
                       G_ADD_PRIVATE(GvaMetaPublishKafka);
                       GST_DEBUG_CATEGORY_INIT(gva_meta_publish_kafka_debug_category, "gvametapublishkafka", 0,
                                               "debug category for gvametapublishkafka element"));

static void gva_meta_publish_kafka_init(GvaMetaPublishKafka *self) {
    // Initialize of private data
    auto *priv_memory = gva_meta_publish_kafka_get_instance_private(self);
    // This won't be converted to shared ptr because of memory placement
    self->impl = new (priv_memory) GvaMetaPublishKafkaPrivate(&self->base);
}

static void gva_meta_publish_kafka_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    auto self = GVA_META_PUBLISH_KAFKA(object);

    if (!self->impl->get_property(prop_id, value))
        G_OBJECT_CLASS(gva_meta_publish_kafka_parent_class)->get_property(object, prop_id, value, pspec);
}

static void gva_meta_publish_kafka_set_property(GObject *object, guint prop_id, const GValue *value,
                                                GParamSpec *pspec) {
    auto self = GVA_META_PUBLISH_KAFKA(object);

    if (!self->impl->set_property(prop_id, value))
        G_OBJECT_CLASS(gva_meta_publish_kafka_parent_class)->set_property(object, prop_id, value, pspec);
}

static void gva_meta_publish_kafka_finalize(GObject *object) {
    auto self = GVA_META_PUBLISH_KAFKA(object);
    g_assert(self->impl && "Expected valid 'impl' pointer during finalize");

    if (self->impl) {
        self->impl->~GvaMetaPublishKafkaPrivate();
        self->impl = nullptr;
    }

    G_OBJECT_CLASS(gva_meta_publish_kafka_parent_class)->finalize(object);
}

static void gva_meta_publish_kafka_class_init(GvaMetaPublishKafkaClass *klass) {
    auto gobject_class = G_OBJECT_CLASS(klass);
    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    auto base_metapublish_class = GVA_META_PUBLISH_BASE_CLASS(klass);

    gobject_class->set_property = gva_meta_publish_kafka_set_property;
    gobject_class->get_property = gva_meta_publish_kafka_get_property;
    gobject_class->finalize = gva_meta_publish_kafka_finalize;

    base_transform_class->start = [](GstBaseTransform *base) { return GVA_META_PUBLISH_KAFKA(base)->impl->start(); };
    base_transform_class->stop = [](GstBaseTransform *base) { return GVA_META_PUBLISH_KAFKA(base)->impl->stop(); };

    base_metapublish_class->publish = [](GvaMetaPublishBase *base, const std::string &message) {
        return GVA_META_PUBLISH_KAFKA(base)->impl->publish(message);
    };

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass), "Kafka metadata publisher", "Metadata",
                                          "Publishes the JSON metadata to Kafka message broker", "Intel Corporation");

    auto prm_flags = static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);
    g_object_class_install_property(
        gobject_class, PROP_ADDRESS,
        g_param_spec_string("address", "Address", "Broker address", DEFAULT_ADDRESS, prm_flags));
    g_object_class_install_property(
        gobject_class, PROP_TOPIC,
        g_param_spec_string("topic", "Topic", "Topic on which to send broker messages", DEFAULT_TOPIC, prm_flags));
    g_object_class_install_property(gobject_class, PROP_MAX_CONNECT_ATTEMPTS,
                                    g_param_spec_uint("max-connect-attempts", "Max Connect Attempts",
                                                      "Maximum number of failed connection "
                                                      "attempts before it is considered fatal.",
                                                      1, 10, DEFAULT_MAX_CONNECT_ATTEMPTS, prm_flags));
    g_object_class_install_property(
        gobject_class, PROP_MAX_RECONNECT_INTERVAL,
        g_param_spec_uint("max-reconnect-interval", "Max Reconnect Interval",
                          "Maximum time in seconds between reconnection attempts. Initial "
                          "interval is 1 second and will be doubled on each failure up to this maximum interval.",
                          1, 300, DEFAULT_MAX_RECONNECT_INTERVAL, prm_flags));
}

static gboolean plugin_init(GstPlugin *plugin) {
    gboolean result = TRUE;
    result &= gst_element_register(plugin, "gvametapublishkafka", GST_RANK_NONE, GST_TYPE_GVA_META_PUBLISH_KAFKA);
    return result;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, gvametapublishkafka,
                  PRODUCT_FULL_NAME " Kafka metapublish element", plugin_init, PLUGIN_VERSION, PLUGIN_LICENSE,
                  PACKAGE_NAME, GST_PACKAGE_ORIGIN)
