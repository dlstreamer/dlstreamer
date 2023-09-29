/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gvametapublishbase.hpp>

#include <librdkafka/rdkafkacpp.h>

#include <cstdint>
#include <memory>
#include <string>

namespace {
constexpr auto MILLISEC_PER_SEC = 1000;
}

/* Properties */
enum {
    PROP_0,
    PROP_ADDRESS,
    PROP_TOPIC,
    PROP_MAX_CONNECT_ATTEMPTS,
    PROP_MAX_RECONNECT_INTERVAL,
};

template <typename ProducerFactory, typename TopicFactory>
class GvaMetaPublishKafkaImpl : public RdKafka::DeliveryReportCb, public RdKafka::EventCb {
  private:
    bool init_kafka_producer() {
        std::unique_ptr<RdKafka::Conf> producerConfig(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

        std::string error;
        if (producerConfig->set("dr_cb", static_cast<RdKafka::DeliveryReportCb *>(this), error) !=
            RdKafka::Conf::CONF_OK) {
            GST_ERROR_OBJECT(_base, "Failed to set Kafka deliver callback: %s", error.c_str());
            return false;
        }
        if (producerConfig->set("event_cb", static_cast<RdKafka::EventCb *>(this), error) != RdKafka::Conf::CONF_OK) {
            GST_ERROR_OBJECT(_base, "Failed to set Kafka event callback: %s", error.c_str());
            return false;
        }

        auto set_conf = [&](const std::string &name, const std::string &value) {
            if (producerConfig->set(name, value, error) != RdKafka::Conf::CONF_OK) {
                GST_ERROR_OBJECT(_base, "Failed to set kafka config property %s: %s", name.c_str(), error.c_str());
                return false;
            }
            return true;
        };

        if (!set_conf("bootstrap.servers", _address))
            return false;
        if (!set_conf("reconnect.backoff.ms", "1000"))
            return false;
        if (!set_conf("reconnect.backoff.max.ms", std::to_string(_max_reconnect_interval * MILLISEC_PER_SEC)))
            return false;

        _producer.reset(ProducerFactory::create(producerConfig.get(), error));
        if (!_producer) {
            GST_ERROR_OBJECT(_base, "Failed to create producer handle: %s", error.c_str());
            return false;
        }

        _kafka_topic.reset(TopicFactory::create(_producer.get(), _topic, nullptr, error));
        if (!_kafka_topic) {
            GST_ERROR_OBJECT(_base, "Failed to create new topic handle: %s", error.c_str());
            return false;
        }

        GST_DEBUG_OBJECT(_base, "Successfully opened connection to Kafka.");
        return true;
    }

  public:
    GvaMetaPublishKafkaImpl(GvaMetaPublishBase *base) : _base(base) {
    }

    ~GvaMetaPublishKafkaImpl() override = default;

    void dr_cb(RdKafka::Message &message) final {
        if (message.err() != RdKafka::ERR_NO_ERROR) {
            GST_ERROR_OBJECT(_base, "Message failed to publish to Kafka. Error message: %s", message.errstr().c_str());
        } else {
            GST_DEBUG_OBJECT(_base, "Message successfully published to Kafka");
        }
    }

    void event_cb(RdKafka::Event &event) final {
        if (event.type() != RdKafka::Event::EVENT_ERROR &&
            !(event.type() == RdKafka::Event::EVENT_LOG && event.severity() <= RdKafka::Event::EVENT_SEVERITY_ERROR)) {
            // Consider errors only if event type is ERROR or it's LOG but with ERROR or higher severity
            return;
        }

        GST_ERROR_OBJECT(_base, "Kafka connection error. attempt: %d code: %d reason: %s ", _connection_attempt,
                         event.err(), event.str().c_str());
        if (_connection_attempt == _max_connect_attempts) {
            GST_ELEMENT_ERROR(_base, RESOURCE, NOT_FOUND,
                              ("Failed to connect to Kafka after maximum configured attempts."), (NULL));
            return;
        }
        _connection_attempt++;
    }

    gboolean start() {
        _connection_attempt = 1;

        if (!init_kafka_producer()) {
            GST_ELEMENT_ERROR(_base, RESOURCE, NOT_FOUND, ("Failed to start"), ("Failed to initialize Kafka producer"));
            return false;
        }

        return true;
    }

    gboolean stop() {
        if (!_producer)
            return true;

        if (_producer->flush(3 * MILLISEC_PER_SEC) != RdKafka::ERR_NO_ERROR) {
            GST_ERROR_OBJECT(_base, "Failed to flush kafka producer.");
            auto queue_size = _producer->outq_len();
            if (queue_size > 0) {
                GST_ERROR_OBJECT(_base, "%d messages were not delivered", queue_size);
            }
        } else {
            GST_DEBUG_OBJECT(_base, "Successfully flushed Kafka producer.");
        }

        return true;
    }

    gboolean publish(const std::string &message) {
        if (!_producer) {
            GST_ERROR_OBJECT(_base, "Producer handler is null. Cannot publish message.");
            return false;
        }
        _producer->poll(0);
        if (_producer->produce(_kafka_topic.get(), RdKafka::Topic::PARTITION_UA, RdKafka::Producer::MSG_COPY,
                               (void *)const_cast<char *>(message.c_str()), message.size(), nullptr, nullptr)) {

            std::string error;
            _producer->fatal_error(error);
            GST_ERROR_OBJECT(_base, "Failed to publish message: %s", error.c_str());
            return false;
        }

        GST_DEBUG_OBJECT(_base, "Kafka message sent.");
        return true;
    }

    bool get_property(guint prop_id, GValue *value) {
        switch (prop_id) {
        case PROP_ADDRESS:
            g_value_set_string(value, _address.c_str());
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

    bool set_property(guint prop_id, const GValue *value) {
        switch (prop_id) {
        case PROP_ADDRESS:
            _address = g_value_get_string(value);
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

  protected:
    GvaMetaPublishBase *_base;

    std::string _address;
    std::string _topic;
    uint32_t _max_connect_attempts = 0;
    uint32_t _max_reconnect_interval = 0;

    std::unique_ptr<RdKafka::Producer> _producer;
    std::unique_ptr<RdKafka::Topic> _kafka_topic;
    uint32_t _connection_attempt = 0;
};
