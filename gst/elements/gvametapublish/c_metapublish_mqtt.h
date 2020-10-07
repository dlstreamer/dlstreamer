/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef __C_METAPUBLISH_MQTT_H__
#define __C_METAPUBLISH_MQTT_H__
#ifdef PAHO_INC

#include "MQTTAsync.h"
#include "i_metapublish_method.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define METAPUBLISH_TYPE_MQTT metapublish_mqtt_get_type()
G_DECLARE_FINAL_TYPE(MetapublishMQTT, metapublish_mqtt, METAPUBLISH, MQTT, GObject)

void connection_lost(void *context, char *cause);
int message_arrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message);
void delivery_complete(void *context, MQTTAsync_token token);
void on_connect_success(void *context, MQTTAsync_successData *response);
void on_connect_failure(void *context, MQTTAsync_failureData *response);
void on_send_success(void *context, MQTTAsync_successData *response);
void on_send_failure(void *context, MQTTAsync_failureData *response);
void on_disconnect_success(void *context, MQTTAsync_successData *response);
void on_disconnect_failure(void *context, MQTTAsync_failureData *response);

G_END_DECLS

#endif /* PAHO_INC */
#endif /* __C_METAPUBLISH_MQTT_H__ */
