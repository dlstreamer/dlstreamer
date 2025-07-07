/*******************************************************************************
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "audio_frame.h"
#include <gtest/gtest.h>

struct AudioFrameTest : public ::testing::Test {

    GstBuffer *buffer;
    GVA::AudioFrame *frame;

    void SetUp() {
        buffer = gst_buffer_new_and_alloc(0);
        GstAudioInfo *info = gst_audio_info_new();
        gst_audio_info_init(info);

        frame = new GVA::AudioFrame(buffer, info);
    }

    void TearDown() {
        if (buffer)
            gst_buffer_unref(buffer);
        if (frame)
            delete frame;
    }
};

TEST_F(AudioFrameTest, AudioFrameTestConstructor) {
    ASSERT_ANY_THROW(new GVA::AudioFrame(nullptr));
    ASSERT_ANY_THROW(new GVA::AudioFrame(buffer));

    GstAudioInfo *info = gst_audio_info_new();
    gst_audio_info_init(info);
    info->rate = 100;
    info->channels = 1;
    info->bpf = 100;
    info->finfo = gst_audio_format_get_info(GST_AUDIO_FORMAT_S16LE);
    GstAudioMeta *meta = gst_buffer_add_audio_meta(buffer, info, 160, NULL);
    GVA::AudioFrame audioFrame(buffer);

    ASSERT_NE(audioFrame.audio_info(), nullptr);
    ASSERT_EQ(audioFrame.audio_info()->rate, info->rate);
    ASSERT_EQ(audioFrame.audio_info()->channels, info->channels);
    ASSERT_EQ(audioFrame.audio_info()->bpf, info->bpf);
    ASSERT_EQ(audioFrame.audio_info()->finfo, info->finfo);
}

TEST_F(AudioFrameTest, AudioFrameTestAddEvents) {
    ASSERT_EQ(frame->events().size(), 0);
    ASSERT_EQ(frame->tensors().size(), 0);

    const constexpr size_t NUM_EVENTS = 100;
    for (int i = 0; i < NUM_EVENTS; i++) {
        frame->add_event(i, i, "Test", i);
    }
    ASSERT_EQ(frame->events().size(), NUM_EVENTS);
}

TEST_F(AudioFrameTest, AudioFrameTestRemoveEventsValid) {
    ASSERT_EQ(frame->events().size(), 0);
    ASSERT_EQ(frame->tensors().size(), 0);

    GVA::AudioEvent event = frame->add_event(1, 2, "Test", 3);
    ASSERT_EQ(frame->events().size(), 1);
    frame->remove_event(event);
    ASSERT_EQ(frame->events().size(), 0);
}

TEST_F(AudioFrameTest, AudioFrameTestRemoveEventsInvalid) {
    ASSERT_EQ(frame->events().size(), 0);
    ASSERT_EQ(frame->tensors().size(), 0);

    // GstGVAAudioEventMeta *meta = gst_gva_buffer_add_audio_event_meta(buffer, label.c_str(), start_time, end_time);
    // GVA::AudioEvent otherEvent = AudioEvent(meta);

    GVA::AudioEvent event = frame->add_event(1, 2, "Test", 3);
    ASSERT_EQ(frame->events().size(), 1);
    frame->remove_event(event);
    ASSERT_EQ(frame->events().size(), 0);
}

TEST_F(AudioFrameTest, AudioFrameTestAddTensors) {
    ASSERT_EQ(frame->tensors().size(), 0);

    const constexpr size_t tensor_meta_size = 10;
    const constexpr char field_name[] = "model_name";
    const constexpr char model_name[] = "test_model";
    for (size_t i = 0; i < tensor_meta_size; ++i) {
        GVA::Tensor tensor = frame->add_tensor();
        std::string test_model = model_name + std::to_string(i);
        gst_structure_set(tensor.gst_structure(), field_name, G_TYPE_STRING, test_model.c_str(), NULL);
    }

    std::vector<GVA::Tensor> frame_tensors = frame->tensors();
    ASSERT_EQ(frame_tensors.size(), tensor_meta_size);

    unsigned idx = 0;
    for (GVA::Tensor &tensor : frame->tensors()) {
        std::string test_model_name = model_name + std::to_string(idx);
        std::vector<GVA::Tensor>::iterator pos_elem =
            std::find_if(frame_tensors.begin(), frame_tensors.end(), [test_model_name, field_name](GVA::Tensor tensor) {
                return (tensor.get_string(field_name) == test_model_name);
            });
        if (pos_elem != frame_tensors.end())
            frame_tensors.erase(pos_elem);
        ++idx;
    }
    ASSERT_EQ(frame_tensors.size(), 0);
}

TEST_F(AudioFrameTest, AudioFrameTestJSONMessages) {
    ASSERT_EQ(frame->messages().size(), 0);

    const constexpr size_t MESSAGES_NUMBER = 10;
    std::vector<std::string> test_messages;
    for (int i = 0; i < MESSAGES_NUMBER; ++i) {
        std::string test_message = "test_message_" + std::to_string(i);
        test_messages.push_back(test_message);
        frame->add_message(test_message);
    }

    ASSERT_EQ(frame->messages().size(), MESSAGES_NUMBER);
    std::vector<std::string> messages = frame->messages();

    for (auto it = messages.begin(); it != messages.end(); ++it) {
        auto pos_mes = std::find(test_messages.begin(), test_messages.end(), *it);
        if (pos_mes != test_messages.end())
            test_messages.erase(pos_mes);
    }
    ASSERT_EQ(test_messages.size(), 0);

    GstGVAJSONMeta *meta = NULL;
    gpointer state = NULL;
    unsigned index = 0;
    while ((meta = GST_GVA_JSON_META_ITERATE(buffer, &state))) {
        char t_message[30];
        sprintf(t_message, "%dtest_message", index + 10);
        set_json_message(meta, t_message);
        ++index;
    }

    for (int i = 0; i < MESSAGES_NUMBER; ++i) {
        std::string test_message = std::to_string(i + MESSAGES_NUMBER) + "test_message";
        test_messages.push_back(test_message);
    }

    messages = frame->messages();
    ASSERT_EQ(messages.size(), MESSAGES_NUMBER);

    for (auto it = messages.begin(); it != messages.end(); ++it) {
        auto pos_mes = std::find(test_messages.begin(), test_messages.end(), *it);
        if (pos_mes != test_messages.end())
            test_messages.erase(pos_mes);
    }
    ASSERT_EQ(test_messages.size(), 0);
}
