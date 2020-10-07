/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "c_metapublish_kafka.h"
#ifdef KAFKA_INC

GST_DEBUG_CATEGORY_STATIC(gst_gva_meta_publish_debug_category);
#define GST_CAT_DEFAULT gst_gva_meta_publish_debug_category
#define MILLISEC_PER_SEC 1000

struct _MetapublishKafka {
    GObject parent_instance;
    rd_kafka_t *producer_handler;
    rd_kafka_topic_t *kafka_topic;
    guint connection_attempt;
    GstGvaMetaPublish *gvametapublish;
};

static void metapublish_kafka_method_interface_init(MetapublishMethodInterface *iface);
static void dr_msg_cb(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque);
static void error_cb(rd_kafka_t *rk, int err, const char *reason, void *opaque);
G_DEFINE_TYPE_WITH_CODE(MetapublishKafka, metapublish_kafka, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(METAPUBLISH_TYPE_METHOD, metapublish_kafka_method_interface_init))

static gboolean metapublish_kafka_method_start(MetapublishMethod *self, GstGvaMetaPublish *gvametapublish) {
    GST_DEBUG_CATEGORY_INIT(gst_gva_meta_publish_debug_category, "gvametapublish", 0,
                            "debug category for gvametapublish element");
    MetapublishKafka *mp_kafka = METAPUBLISH_KAFKA(self);
    mp_kafka->gvametapublish = gvametapublish;
    mp_kafka->connection_attempt = 1;
    rd_kafka_conf_t *producerConfig;
    gchar error_msg[512];
    producerConfig = rd_kafka_conf_new();

    gchar max_reconnect_interval[sizeof(guint) * 8 + 1];
    sprintf(max_reconnect_interval, "%u", (gvametapublish->max_reconnect_interval * MILLISEC_PER_SEC));
    rd_kafka_conf_set_opaque(producerConfig, mp_kafka);

    rd_kafka_conf_set_dr_msg_cb(producerConfig, dr_msg_cb);
    rd_kafka_conf_set_error_cb(producerConfig, error_cb);
    if (rd_kafka_conf_set(producerConfig, "bootstrap.servers", gvametapublish->address, error_msg, sizeof(error_msg)) !=
        RD_KAFKA_CONF_OK) {
        if (producerConfig) {
            rd_kafka_conf_destroy(producerConfig);
        }
        GST_ERROR_OBJECT(gvametapublish, "Failed to set kafka config property. %s", error_msg);
        return FALSE;
    }
    if (rd_kafka_conf_set(producerConfig, "reconnect.backoff.ms", "1000", error_msg, sizeof(error_msg)) !=
        RD_KAFKA_CONF_OK) {
        if (producerConfig) {
            rd_kafka_conf_destroy(producerConfig);
        }
        GST_ERROR_OBJECT(gvametapublish, "Failed to set kafka config property. %s", error_msg);
        return FALSE;
    }
    if (rd_kafka_conf_set(producerConfig, "reconnect.backoff.max.ms", max_reconnect_interval, error_msg,
                          sizeof(error_msg)) != RD_KAFKA_CONF_OK) {
        if (producerConfig) {
            rd_kafka_conf_destroy(producerConfig);
        }
        GST_ERROR_OBJECT(gvametapublish, "Failed to set kafka config property. %s", error_msg);
        return FALSE;
    }

    mp_kafka->producer_handler = rd_kafka_new(RD_KAFKA_PRODUCER, producerConfig, error_msg, sizeof(error_msg));
    if (!mp_kafka->producer_handler) {
        GST_ERROR_OBJECT(gvametapublish, "Failed to create producer handle. %s", error_msg);
        return FALSE;
    }

    mp_kafka->kafka_topic = rd_kafka_topic_new(mp_kafka->producer_handler, gvametapublish->topic, NULL);
    if (!mp_kafka->kafka_topic) {
        rd_kafka_destroy(mp_kafka->producer_handler);
        GST_ERROR_OBJECT(gvametapublish, "Failed to create new topic handle.");
        return FALSE;
    }
    GST_DEBUG_OBJECT(gvametapublish, "Successfully opened connection to Kafka.");
    return TRUE;
}

static gboolean metapublish_kafka_method_publish(MetapublishMethod *self, GstGvaMetaPublish *gvametapublish,
                                                 gchar *json_message) {
    MetapublishKafka *mp_kafka = METAPUBLISH_KAFKA(self);
    if (!mp_kafka->producer_handler) {
        GST_ERROR_OBJECT(gvametapublish, "Producer handler is null. Cannot publish message.");
        return FALSE;
    }
    rd_kafka_poll(mp_kafka->producer_handler, 0);
    if (!json_message) {
        GST_DEBUG_OBJECT(gvametapublish, "No JSON message.");
        return TRUE;
    }
    if (rd_kafka_produce(mp_kafka->kafka_topic, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY, json_message,
                         strlen(json_message), NULL, 0, NULL) != 0) {
        GST_ERROR_OBJECT(gvametapublish, "Failed to publish message. %s", rd_kafka_err2str(rd_kafka_last_error()));
        return FALSE;
    }

    GST_DEBUG_OBJECT(gvametapublish, "Kafka message sent.");
    return TRUE;
}

static gboolean metapublish_kafka_method_stop(MetapublishMethod *self, GstGvaMetaPublish *gvametapublish) {
    MetapublishKafka *mp_kafka = METAPUBLISH_KAFKA(self);
    if (rd_kafka_flush(mp_kafka->producer_handler, 3 * 1000) != RD_KAFKA_RESP_ERR_NO_ERROR) {
        GST_ERROR_OBJECT(gvametapublish, "Failed to flush kafka producer.");
        if (rd_kafka_outq_len(mp_kafka->producer_handler) > 0) {
            GST_ERROR_OBJECT(gvametapublish, "%d messages were not delivered",
                             rd_kafka_outq_len(mp_kafka->producer_handler));
        }
        return TRUE;
    }
    GST_DEBUG_OBJECT(gvametapublish, "Successfully flushed Kafka producer.");
    return TRUE;
}

static void metapublish_kafka_finalize(GObject *gobject) {
    MetapublishKafka *mp_kafka = metapublish_kafka_get_instance_private(METAPUBLISH_KAFKA(gobject));
    rd_kafka_poll(mp_kafka->producer_handler, 0);
    rd_kafka_topic_destroy(mp_kafka->kafka_topic);
    rd_kafka_destroy(mp_kafka->producer_handler);
    if (rd_kafka_wait_destroyed(1 * 1000) == 0) {
        GST_DEBUG_OBJECT(mp_kafka->gvametapublish, "Successfully destroyed Kafka client.");
    } else {
        GST_ERROR_OBJECT(mp_kafka->gvametapublish, "Could not destroy Kafka client.");
    }
    mp_kafka->gvametapublish = NULL;
    G_OBJECT_CLASS(metapublish_kafka_parent_class)->finalize(gobject);
}

static void metapublish_kafka_method_interface_init(MetapublishMethodInterface *iface) {
    iface->start = metapublish_kafka_method_start;
    iface->publish = metapublish_kafka_method_publish;
    iface->stop = metapublish_kafka_method_stop;
}

static void metapublish_kafka_init(MetapublishKafka *self) {
    (void)self;
}

static void metapublish_kafka_class_init(MetapublishKafkaClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = metapublish_kafka_finalize;
}

// Kafka Callbacks
static void dr_msg_cb(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque) {
    if (!rkmessage) {
        GST_ERROR("Message callback received null message");
        return;
    }
    if (rkmessage->err) {
        GST_ERROR("Message failed to publish to Kafka. Error message: %s", rd_kafka_err2str(rkmessage->err));
    } else {
        GST_DEBUG("Message successfully published to Kafka");
    }
    (void)rk;
    (void)opaque;
}

static void error_cb(rd_kafka_t *rk, int err, const char *reason, void *opaque) {
    MetapublishKafka *mp_kafka = (MetapublishKafka *)opaque;
    if (!mp_kafka) {
        GST_ERROR("Error callback received null opaque value.");
        return;
    }
    GST_ERROR("Kafka connection error. attempt: %d code: %d reason: %s ", mp_kafka->connection_attempt, err, reason);
    if (!mp_kafka->gvametapublish) {
        GST_ERROR("The instance of gvametapublish is not available. Maybe it was destroyed");
        return;
    }
    if (mp_kafka->connection_attempt == mp_kafka->gvametapublish->max_connect_attempts) {
        GST_ELEMENT_ERROR(mp_kafka->gvametapublish, RESOURCE, NOT_FOUND,
                          ("Failed to connect to Kafka after maximum configured attempts."), (NULL));
        return;
    }
    mp_kafka->connection_attempt++;
    (void)rk;
}

#endif /* KAFKA_INC */