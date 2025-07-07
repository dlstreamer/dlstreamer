/*******************************************************************************
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gva_audio_event_meta.h"
#include <gtest/gtest.h>

struct AudioEventMetaTest : public ::testing::Test {

    GstBuffer *buffer;

    void SetUp() {
        buffer = gst_buffer_new_and_alloc(0);
    }

    void TearDown() {
        if (buffer)
            gst_buffer_unref(buffer);
    }
};

TEST_F(AudioEventMetaTest, AudioEventMetaTestGetAudioEventMetaId) {
    const constexpr int target_id = 123;
    ASSERT_EQ(gst_gva_buffer_get_audio_event_meta_id(buffer, target_id), nullptr);
    GstGVAAudioEventMeta *meta = gst_gva_buffer_add_audio_event_meta_id(buffer, 1, 2, 3);
    ASSERT_NE(meta, nullptr);
    meta->id = target_id;
    ASSERT_NE(gst_gva_buffer_get_audio_event_meta_id(buffer, target_id), nullptr);
}

TEST_F(AudioEventMetaTest, AudioEventMetaTestAddAudioEventMeta) {
    GstGVAAudioEventMeta *meta = gst_gva_buffer_add_audio_event_meta(buffer, "gint", 2, 3);
    ASSERT_NE(meta, nullptr);
    ASSERT_EQ(meta->start_timestamp, 2);
    ASSERT_EQ(meta->end_timestamp, 3);
    ASSERT_EQ(meta->event_type, 10);
}

TEST_F(AudioEventMetaTest, AudioEventMetaTestAddGetParam) {
    constexpr char nameString[] = "nameString";
    GstStructure *structure = gst_structure_new_empty(nameString);
    GstGVAAudioEventMeta *meta = gst_gva_buffer_add_audio_event_meta_id(buffer, 1, 2, 3);
    ASSERT_EQ(gst_gva_audio_event_meta_get_param(meta, "test"), nullptr);

    gst_gva_audio_event_meta_add_param(meta, structure);

    ASSERT_NE(gst_gva_audio_event_meta_get_param(meta, nameString), nullptr);
}
