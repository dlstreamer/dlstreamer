/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __GVA_AUDIO_EVENT_META_H__
#define __GVA_AUDIO_EVENT_META_H__

#include <gst/gst.h>
#define GVA_AUDIO_EVENT_META_API_NAME "GstGVAAudioEventMetaAPI"
#define GVA_AUDIO_EVENT_META_IMPL_NAME "GstGVAAudioEventMeta"
#define GVA_AUDIO_EVENT_META_TAG "gva_audio_event_meta"

G_BEGIN_DECLS

/**
 * GstGVAAudioEventMeta:
 * @meta: parent #GstMeta
 * @event_type: GQuark describing the semantic of the AudioEvent (f.i. a Sound, a Speech, Silence)
 * @id: identifier of this particular event
 * @start_timestamp: start time stamp of the segment
 * @end_timestamp: end time stamp of the segment
 * @params: list of #GstStructure containing element-specific params for downstream, see
 * gst_gva_audio_event_meta_add_param().
 *
 * Extra buffer metadata describing an audio frame event details
 */
typedef struct {
    GstMeta meta;
    GQuark event_type;
    gint id;
    gulong start_timestamp;
    gulong end_timestamp;

    GList *params;
} GstGVAAudioEventMeta;

GType gst_gva_audio_event_meta_api_get_type(void);
#define GST_GVA_AUDIO_EVENT_META_API_TYPE (gst_gva_audio_event_meta_api_get_type())

const GstMetaInfo *gst_gva_audio_event_meta_get_info(void);
#define GST_GVA_AUDIO_EVENT_META_INFO (gst_gva_audio_event_meta_get_info())

#define gst_gva_buffer_get_audio_event_meta(b)                                                                         \
    ((GvaAudioEventMeta *)gst_buffer_get_meta((b), GST_GVA_AUDIO_EVENT_META_API_TYPE))

GstGVAAudioEventMeta *gst_gva_buffer_get_audio_event_meta_id(GstBuffer *buffer, gint id);

GstGVAAudioEventMeta *gst_gva_buffer_add_audio_event_meta(GstBuffer *buffer, const gchar *event_type,
                                                          gulong start_timestamp, gulong end_timestamp);

GstGVAAudioEventMeta *gst_gva_buffer_add_audio_event_meta_id(GstBuffer *buffer, GQuark event_type,
                                                             gulong start_timestamp, gulong end_timestamp);
void gst_gva_audio_event_meta_add_param(GstGVAAudioEventMeta *meta, GstStructure *s);

GstStructure *gst_gva_audio_event_meta_get_param(GstGVAAudioEventMeta *meta, const gchar *name);

G_END_DECLS

#endif /* __GVA_AUDIO_EVENT_META_H__*/
