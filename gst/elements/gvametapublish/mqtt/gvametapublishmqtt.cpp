/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvametapublishmqtt.hpp"

#include <common.hpp>
#include <safe_arithmetic.hpp>

#include <MQTTAsync.h>
#include <uuid/uuid.h>

#include <cstdint>
#include <string>
#include <thread>

GST_DEBUG_CATEGORY_STATIC(gva_meta_publish_mqtt_debug_category);
#define GST_CAT_DEFAULT gva_meta_publish_mqtt_debug_category

namespace {
std::string generate_client_id() {
    uuid_t binuuid;
    uuid_generate_random(binuuid);
    char uuid[37];
    // 36 character UUID string plus terminating character
    uuid_unparse(binuuid, uuid);
    return uuid;
}

} // namespace

/* Properties */
enum {
    PROP_0,
    PROP_ADDRESS,
    PROP_CLIENT_ID,
    PROP_TOPIC,
    PROP_MAX_CONNECT_ATTEMPTS,
    PROP_MAX_RECONNECT_INTERVAL,
};

class GvaMetaPublishMqttPrivate {
  private:
    // MQTT CALLBACKS
    void on_connect_success(MQTTAsync_successData * /*response*/) {
        GST_DEBUG_OBJECT(_base, "Successfully connected to MQTT");
    }

    void on_connect_failure(MQTTAsync_failureData * /*response*/) {
        GST_WARNING_OBJECT(_base, "Connection attempt to MQTT failed.");
        try_reconnect();
    }

    void on_connection_lost(char *cause) {
        GST_WARNING_OBJECT(_base, "Connection to MQTT lost. Cause: %s. Attempting to reconnect", cause);
        try_reconnect();
    }

    int on_message_arrived(char * /*topicName*/, int /*topicLen*/, MQTTAsync_message * /*message*/) {
        return TRUE;
    }

    void on_delivery_complete(MQTTAsync_token /*token*/) {
    }

    void on_send_success(MQTTAsync_successData * /*response*/) {
        GST_DEBUG_OBJECT(_base, "Message successfully published to MQTT");
    }

    void on_send_failure(MQTTAsync_failureData * /*response*/) {
        GST_ERROR_OBJECT(_base, "Message failed to publish to MQTT");
    }

    void on_disconnect_success(MQTTAsync_successData * /*response*/) {
        GST_DEBUG_OBJECT(_base, "Successfully disconnected from MQTT.");
    }

    void on_disconnect_failure(MQTTAsync_failureData * /*response*/) {
        GST_ERROR_OBJECT(_base, "Failed to disconnect from MQTT.");
    }

    void try_reconnect() {
        if (_connection_attempt == _max_connect_attempts) {
            GST_ELEMENT_ERROR(_base, RESOURCE, NOT_FOUND,
                              ("Failed to connect to MQTT after maximum configured attempts."), (nullptr));
            return;
        }
        _connection_attempt++;
        _sleep_time = std::min(2 * _sleep_time, _max_reconnect_interval);

        std::this_thread::sleep_for(std::chrono::seconds(_sleep_time));
        GST_DEBUG_OBJECT(_base, "Attempt %d to connect to MQTT.", _connection_attempt);
        auto c = MQTTAsync_connect(_client, &_connect_options);
        if (c != MQTTASYNC_SUCCESS) {
            GST_ERROR_OBJECT(_base, "Failed to start connection attempt to MQTT. Error code %d.", c);
        }
    }

  public:
    GvaMetaPublishMqttPrivate(GvaMetaPublishBase *parent) : _base(parent) {
        _connect_options = MQTTAsync_connectOptions_initializer;
        _connect_options.keepAliveInterval = 20;
        _connect_options.cleansession = 1;
        _connect_options.context = this;
        _connect_options.onSuccess = [](void *context, MQTTAsync_successData *response) {
            if (!context) {
                GST_ERROR("Got null context on mqtt connect_success callback");
                return;
            }
            static_cast<GvaMetaPublishMqttPrivate *>(context)->on_connect_success(response);
        };
        _connect_options.onFailure = [](void *context, MQTTAsync_failureData *response) {
            if (!context) {
                GST_ERROR("Got null context on mqtt connect_failure callback");
                return;
            }
            static_cast<GvaMetaPublishMqttPrivate *>(context)->on_connect_failure(response);
        };

        _disconnect_options = MQTTAsync_disconnectOptions_initializer;
        _disconnect_options.context = this;
        _disconnect_options.onSuccess = [](void *context, MQTTAsync_successData *response) {
            if (!context) {
                GST_ERROR("Got null context on mqtt disconnect_success callback");
                return;
            }
            static_cast<GvaMetaPublishMqttPrivate *>(context)->on_disconnect_success(response);
        };
        _disconnect_options.onFailure = [](void *context, MQTTAsync_failureData *response) {
            if (!context) {
                GST_ERROR("Got null context on mqtt disconnect_failure callback");
                return;
            }
            static_cast<GvaMetaPublishMqttPrivate *>(context)->on_disconnect_failure(response);
        };
    }

    ~GvaMetaPublishMqttPrivate() {
        MQTTAsync_destroy(&_client);
        GST_DEBUG("Successfully freed MQTT client.");
    }

    gboolean start() {
        if (_client_id.empty())
            _client_id = generate_client_id();
        _connection_attempt = 1;
        _sleep_time = 1;

        auto sts =
            MQTTAsync_create(&_client, _address.c_str(), _client_id.c_str(), MQTTCLIENT_PERSISTENCE_NONE, nullptr);
        if (sts != MQTTASYNC_SUCCESS) {
            GST_ERROR_OBJECT(_base, "Failed to create MQTTAsync handler. Error code: %d", sts);
            return false;
        }

        sts = MQTTAsync_setCallbacks(_client, this,
                                     [](void *context, char *cause) {
                                         if (!context) {
                                             GST_ERROR("Got null context on mqtt connect_lost callback");
                                             return;
                                         }
                                         static_cast<GvaMetaPublishMqttPrivate *>(context)->on_connection_lost(cause);
                                     },
                                     [](void *context, char *topicName, int topicLen, MQTTAsync_message *message) {
                                         if (!context) {
                                             GST_ERROR("Got null context on mqtt message_arrived callback");
                                             return 0;
                                         }
                                         return static_cast<GvaMetaPublishMqttPrivate *>(context)->on_message_arrived(
                                             topicName, topicLen, message);
                                     },
                                     [](void *context, MQTTAsync_token token) {
                                         if (!context) {
                                             GST_ERROR("Got null context on mqtt delivery_complete callback");
                                             return;
                                         }
                                         static_cast<GvaMetaPublishMqttPrivate *>(context)->on_delivery_complete(token);
                                     });
        if (sts != MQTTASYNC_SUCCESS) {
            GST_ERROR_OBJECT(_base, "Failed to set callbacks for MQTTAsync handler. Error code: %d", sts);
            return false;
        }

        auto c = MQTTAsync_connect(_client, &_connect_options);
        if (c != MQTTASYNC_SUCCESS) {
            GST_ERROR_OBJECT(_base, "Failed to start connection attempt to MQTT. Error code %d.", c);
            return false;
        }
        GST_DEBUG_OBJECT(_base, "Connect request sent to MQTT.");
        return true;
    }

    gboolean publish(const std::string &message) {
        MQTTAsync_message mqtt_message = MQTTAsync_message_initializer;
        mqtt_message.payload = const_cast<char *>(message.c_str());
        mqtt_message.payloadlen = safe_convert<int>(message.size());
        mqtt_message.retained = FALSE;
        // TODO Validate message is JSON
        MQTTAsync_responseOptions ro = MQTTAsync_responseOptions_initializer;
        ro.context = this;
        ro.onSuccess = [](void *context, MQTTAsync_successData *response) {
            if (!context) {
                GST_ERROR("Got null context on mqtt success callback");
                return;
            }
            static_cast<GvaMetaPublishMqttPrivate *>(context)->on_send_success(response);
        };
        ro.onFailure = [](void *context, MQTTAsync_failureData *response) {
            if (!context) {
                GST_ERROR("Got null context on mqtt failure callback");
                return;
            }
            static_cast<GvaMetaPublishMqttPrivate *>(context)->on_send_failure(response);
        };

        auto c = MQTTAsync_sendMessage(_client, _topic.c_str(), &mqtt_message, &ro);
        if (c != MQTTASYNC_SUCCESS) {
            GST_ERROR_OBJECT(_base, "Message was not accepted for publication. Error code %d.", c);
            return true;
        }
        GST_DEBUG_OBJECT(_base, "MQTT message sent.");
        return true;
    }

    gboolean stop() {
        if (!MQTTAsync_isConnected(_client)) {
            GST_DEBUG_OBJECT(_base, "MQTT client is not connected. Nothing to disconnect");
            return true;
        }

        auto c = MQTTAsync_disconnect(_client, &_disconnect_options);
        if (c != MQTTASYNC_SUCCESS) {
            GST_ERROR_OBJECT(_base, "Disconnection from MQTT failed with error code %d.", c);
            return false;
        }
        GST_DEBUG_OBJECT(_base, "Disconnect request sent to MQTT.");
        return true;
    }

    bool set_property(guint prop_id, const GValue *value) {
        switch (prop_id) {
        case PROP_ADDRESS:
            _address = g_value_get_string(value);
            break;
        case PROP_CLIENT_ID:
            _client_id = g_value_get_string(value);
            break;
        case PROP_TOPIC:
            _topic = g_value_get_string(value);
            break;
        case PROP_MAX_CONNECT_ATTEMPTS:
            _max_connect_attempts = g_value_get_uint(value);
            break;
        case PROP_MAX_RECONNECT_INTERVAL:
            _max_reconnect_interval = g_value_get_uint(value);
            break;
        default:
            return false;
        }

        return true;
    }

    bool get_property(guint prop_id, GValue *value) {
        switch (prop_id) {
        case PROP_ADDRESS:
            g_value_set_string(value, _address.c_str());
            break;
        case PROP_CLIENT_ID:
            g_value_set_string(value, _client_id.c_str());
            break;
        case PROP_TOPIC:
            g_value_set_string(value, _topic.c_str());
            break;
        case PROP_MAX_CONNECT_ATTEMPTS:
            g_value_set_uint(value, _max_connect_attempts);
            break;
        case PROP_MAX_RECONNECT_INTERVAL:
            g_value_set_uint(value, _max_reconnect_interval);
            break;
        default:
            return false;
        }
        return true;
    }

  private:
    GvaMetaPublishBase *_base;

    std::string _address;
    std::string _client_id;
    std::string _topic;
    uint32_t _max_connect_attempts;
    uint32_t _max_reconnect_interval;

    MQTTAsync _client;
    MQTTAsync_connectOptions _connect_options;
    MQTTAsync_disconnectOptions _disconnect_options;
    uint32_t _connection_attempt;
    uint32_t _sleep_time;
};

G_DEFINE_TYPE_EXTENDED(GvaMetaPublishMqtt, gva_meta_publish_mqtt, GST_TYPE_GVA_META_PUBLISH_BASE, 0,
                       G_ADD_PRIVATE(GvaMetaPublishMqtt);
                       GST_DEBUG_CATEGORY_INIT(gva_meta_publish_mqtt_debug_category, "gvametapublishmqtt", 0,
                                               "debug category for gvametapublishmqtt element"));

static void gva_meta_publish_mqtt_init(GvaMetaPublishMqtt *self) {
    // Initialize of private data
    auto *priv_memory = gva_meta_publish_mqtt_get_instance_private(self);
    self->impl = new (priv_memory) GvaMetaPublishMqttPrivate(&self->base);
}

static void gva_meta_publish_mqtt_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    auto self = GVA_META_PUBLISH_MQTT(object);

    if (!self->impl->get_property(prop_id, value))
        G_OBJECT_CLASS(gva_meta_publish_mqtt_parent_class)->get_property(object, prop_id, value, pspec);
}

static void gva_meta_publish_mqtt_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    auto self = GVA_META_PUBLISH_MQTT(object);

    if (!self->impl->set_property(prop_id, value))
        G_OBJECT_CLASS(gva_meta_publish_mqtt_parent_class)->set_property(object, prop_id, value, pspec);
}

static void gva_meta_publish_mqtt_finalize(GObject *object) {
    auto self = GVA_META_PUBLISH_MQTT(object);
    g_assert(self->impl && "Expected valid 'impl' pointer during finalize");

    if (self->impl) {
        self->impl->~GvaMetaPublishMqttPrivate();
        self->impl = nullptr;
    }

    G_OBJECT_CLASS(gva_meta_publish_mqtt_parent_class)->finalize(object);
}

static void gva_meta_publish_mqtt_class_init(GvaMetaPublishMqttClass *klass) {
    auto gobject_class = G_OBJECT_CLASS(klass);
    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    auto base_metapublish_class = GVA_META_PUBLISH_BASE_CLASS(klass);

    gobject_class->set_property = gva_meta_publish_mqtt_set_property;
    gobject_class->get_property = gva_meta_publish_mqtt_get_property;
    gobject_class->finalize = gva_meta_publish_mqtt_finalize;

    base_transform_class->start = [](GstBaseTransform *base) { return GVA_META_PUBLISH_MQTT(base)->impl->start(); };
    base_transform_class->stop = [](GstBaseTransform *base) { return GVA_META_PUBLISH_MQTT(base)->impl->stop(); };

    base_metapublish_class->publish = [](GvaMetaPublishBase *base, const std::string &message) {
        return GVA_META_PUBLISH_MQTT(base)->impl->publish(message);
    };

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass), "Mqtt metadata publisher", "Metadata",
                                          "Publishes the JSON metadata to MQTT message broker", "Intel Corporation");

    auto prm_flags = static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);
    g_object_class_install_property(
        gobject_class, PROP_ADDRESS,
        g_param_spec_string("address", "Address", "Broker address", DEFAULT_ADDRESS, prm_flags));
    g_object_class_install_property(gobject_class, PROP_CLIENT_ID,
                                    g_param_spec_string("client-id", "MQTT Client ID",
                                                        "Unique identifier for the MQTT "
                                                        "client. If not provided, one will be generated for you.",
                                                        DEFAULT_MQTTCLIENTID, prm_flags));
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
    result &= gst_element_register(plugin, "gvametapublishmqtt", GST_RANK_NONE, GST_TYPE_GVA_META_PUBLISH_MQTT);
    return result;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, gvametapublishmqtt,
                  PRODUCT_FULL_NAME " MQTT metapublish element", plugin_init, PLUGIN_VERSION, PLUGIN_LICENSE,
                  PACKAGE_NAME, GST_PACKAGE_ORIGIN)
