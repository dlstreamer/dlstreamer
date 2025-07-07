/*******************************************************************************
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <gvametapublishkafka.hpp>
#include <gvametapublishkafkaimpl.hpp>

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define STUB_METHOD(method, ret, ...)                                                                                  \
    ret method(__VA_ARGS__) final {                                                                                    \
        throw std::runtime_error("The stub for '" #method " (" #__VA_ARGS__ ") "                                       \
                                 "' has been called unexpectedly");                                                    \
    }
#define STUB_METHOD_CONST(method, ret, ...)                                                                            \
    ret method(__VA_ARGS__) const final {                                                                              \
        throw std::runtime_error("The stub for '" #method " (" #__VA_ARGS__ ") "                                       \
                                 "' has been called unexpectedly");                                                    \
    }
#define STUB_METHOD_VIRTUAL(method, ret, ...)                                                                          \
    ret method(__VA_ARGS__) const override {                                                                           \
        throw std::runtime_error("The stub for '" #method " (" #__VA_ARGS__ ") "                                       \
                                 "' has been called unexpectedly");                                                    \
    }

using namespace RdKafka;

class MockMessage : public Message {
  public:
    MOCK_CONST_METHOD0(errstr, std::string());
    MOCK_CONST_METHOD0(err, ErrorCode());
    MOCK_CONST_METHOD0(topic, Topic *());
    MOCK_CONST_METHOD0(topic_name, std::string());
    MOCK_CONST_METHOD0(partition, int32_t());
    MOCK_CONST_METHOD0(payload, void *());
    MOCK_CONST_METHOD0(len, size_t());
    MOCK_CONST_METHOD0(key, const std::string *());
    MOCK_CONST_METHOD0(key_pointer, const void *());
    MOCK_CONST_METHOD0(key_len, size_t());
    MOCK_CONST_METHOD0(offset, int64_t());
    MOCK_CONST_METHOD0(timestamp, MessageTimestamp());
    MOCK_CONST_METHOD0(msg_opaque, void *());
    MOCK_CONST_METHOD0(latency, int64_t());
    MOCK_METHOD0(c_ptr, struct rd_kafka_message_s *());
    MOCK_CONST_METHOD0(status, Status());
    MOCK_METHOD0(headers, RdKafka::Headers *());
    MOCK_METHOD1(headers, RdKafka::Headers *(RdKafka::ErrorCode *err));
    MOCK_CONST_METHOD0(broker_id, int32_t());

    int32_t leader_epoch() const override {
        return 0;
    }
    RdKafka::Error *offset_store() override {
        return nullptr;
    }
};

class MockEvent : public Event {
  public:
    MOCK_CONST_METHOD0(type, Type());
    MOCK_CONST_METHOD0(err, ErrorCode());
    MOCK_CONST_METHOD0(severity, Severity());
    MOCK_CONST_METHOD0(fac, std::string());
    MOCK_CONST_METHOD0(str, std::string());
    MOCK_CONST_METHOD0(throttle_time, int());
    MOCK_CONST_METHOD0(broker_name, std::string());
    MOCK_CONST_METHOD0(broker_id, int());
    MOCK_CONST_METHOD0(fatal, bool());
};

class MockTopic : public Topic {
  public:
    static MockTopic *create(RdKafka::Handle *, const std::string &, const RdKafka::Conf *, std::string &) {
        return new MockTopic();
    }

    STUB_METHOD_VIRTUAL(name, std::string, );
    STUB_METHOD_CONST(partition_available, bool, int32_t);
    STUB_METHOD(offset_store, ErrorCode, int32_t, int64_t);
    STUB_METHOD(c_ptr, struct rd_kafka_topic_s *, );
};

class MockProducer : public Producer {
  public:
    static MockProducer *create(RdKafka::Conf *conf, std::string &error) {
        auto mock = new MockProducer();
        EXPECT_TRUE(conf != nullptr) << "Expected non-null RdKafka::Conf instance when creating producer";
        if (conf) {
            EXPECT_EQ(conf->get(mock->dr_msg_cb), Conf::CONF_OK) << "Expected dr_msg_cb set in RdKafka::Conf";
            EXPECT_EQ(conf->get(mock->event_cb), Conf::CONF_OK) << "Expected event_cb set in RdKafka::Conf";
        }
        return mock;
    }

    ~MockProducer() final = default;

    MOCK_METHOD7(produce, ErrorCode(Topic *topic, int32_t partition, int msgflags, void *payload, size_t len,
                                    const std::string *key, void *msg_opaque));
    MOCK_METHOD1(flush, ErrorCode(int timeout_ms));

    MOCK_METHOD1(poll, int(int timeout_ms));
    MOCK_METHOD0(outq_len, int());
    MOCK_CONST_METHOD1(fatal_error, ErrorCode(std::string &errstr));

    // STUBS
    STUB_METHOD(produce, ErrorCode, Topic *, int32_t, int, void *, size_t, const void *, size_t, void *);
    STUB_METHOD(produce, ErrorCode, const std::string, int32_t, int, void *, size_t, const void *, size_t, int64_t,
                void *);
    STUB_METHOD(produce, ErrorCode, const std::string, int32_t, int, void *, size_t, const void *, size_t, int64_t,
                RdKafka::Headers *, void *);
    STUB_METHOD(produce, ErrorCode, Topic *, int32_t, const std::vector<char> *, const std::vector<char> *, void *);

    STUB_METHOD(purge, ErrorCode, int);
    STUB_METHOD(init_transactions, Error *, int);
    STUB_METHOD(begin_transaction, Error *, );
    STUB_METHOD(send_offsets_to_transaction, Error *, const std::vector<TopicPartition *> &,
                const ConsumerGroupMetadata *, int);
    STUB_METHOD(commit_transaction, Error *, int);
    STUB_METHOD(abort_transaction, Error *, int);
    STUB_METHOD_CONST(name, std::string, );
    STUB_METHOD_CONST(memberid, std::string, );
    STUB_METHOD(metadata, ErrorCode, bool, const Topic *, Metadata **, int);
    STUB_METHOD(pause, ErrorCode, std::vector<TopicPartition *> &);
    STUB_METHOD(resume, ErrorCode, std::vector<TopicPartition *> &);
    STUB_METHOD(query_watermark_offsets, ErrorCode, const std::string &, int32_t, int64_t *, int64_t *, int);
    STUB_METHOD(get_watermark_offsets, ErrorCode, const std::string &, int32_t, int64_t *, int64_t *);
    STUB_METHOD(offsetsForTimes, ErrorCode, std::vector<TopicPartition *> &, int);
    STUB_METHOD(get_partition_queue, Queue *, const TopicPartition *);
    STUB_METHOD(set_log_queue, ErrorCode, Queue *);
    STUB_METHOD(yield, void, );
    STUB_METHOD(clusterid, std::string, int);
    STUB_METHOD(c_ptr, struct rd_kafka_s *, );
    STUB_METHOD(controllerid, int32_t, int);
    STUB_METHOD(oauthbearer_set_token, ErrorCode, const std::string &, int64_t, const std::string &,
                const std::list<std::string> &, std::string &);
    STUB_METHOD(oauthbearer_set_token_failure, ErrorCode, const std::string &);

    RdKafka::DeliveryReportCb *dr_msg_cb;
    RdKafka::EventCb *event_cb;

    void *mem_malloc(size_t) override {
        return nullptr;
    }
    void mem_free(void *) override {
    }
    RdKafka::Error *sasl_background_callbacks_enable() override {
        return nullptr;
    }
    RdKafka::Queue *get_sasl_queue() override {
        return nullptr;
    }
    RdKafka::Queue *get_background_queue() override {
        return nullptr;
    }
    RdKafka::Error *sasl_set_credentials(const std::string &, const std::string &) override {
        return nullptr;
    }
};

class MockProducerFail {
  public:
    static Producer *create(RdKafka::Conf *conf, std::string &error) {
        error = "Failed by test";
        return nullptr;
    }
};

class GvaMetaPublishKafkaImplMocked : public GvaMetaPublishKafkaImpl<MockProducer, MockTopic> {
  public:
    GvaMetaPublishKafkaImplMocked(GvaMetaPublishBase *base) : GvaMetaPublishKafkaImpl<MockProducer, MockTopic>(base) {
    }
    ~GvaMetaPublishKafkaImplMocked() final = default;

    MockProducer *get_mock_producer() const {
        return static_cast<MockProducer *>(_producer.get());
    }
    MockTopic *get_mock_topic() const {
        return static_cast<MockTopic *>(_kafka_topic.get());
    }
    uint32_t get_connection_attempt() const {
        return _connection_attempt;
    }
};

class GvaMetaPublishKafkaImplFixture : public ::testing::Test {
  protected:
    void SetUp() final {
        _element = reinterpret_cast<GvaMetaPublishKafka *>(gst_element_factory_make("gvametapublishkafka", nullptr));
        ASSERT_TRUE(_element != nullptr) << "Expected non-null 'gvametapublishkafka' element created";
        inst.reset(new GvaMetaPublishKafkaImplMocked(&_element->base));
        inst_fail.reset(new GvaMetaPublishKafkaImpl<MockProducerFail, MockTopic>(&_element->base));
    }

    void TearDown() final {
        inst.reset();
        inst_fail.reset();
        EXPECT_TRUE(_element != nullptr) << "Expected non-null 'gvametapublishkafka' element on TearDown";
        g_object_unref(_element);
    }

    std::unique_ptr<GvaMetaPublishKafkaImplMocked> inst;
    std::unique_ptr<GvaMetaPublishKafkaImpl<MockProducerFail, MockTopic>> inst_fail;
    GvaMetaPublishKafka *_element;
};

TEST_F(GvaMetaPublishKafkaImplFixture, test_element_init) {
    // initted in SetUp TearDown
}

TEST_F(GvaMetaPublishKafkaImplFixture, test_start_stop) {
    ASSERT_TRUE(inst->start());
    auto mock = inst->get_mock_producer();
    EXPECT_CALL(*mock, flush).Times(1).WillOnce(::testing::Return(ErrorCode::ERR_NO_ERROR));
    EXPECT_TRUE(inst->stop());
}

TEST_F(GvaMetaPublishKafkaImplFixture, test_start_fail) {
    EXPECT_FALSE(inst_fail->start()) << "Expected failed start since producer is not created";
    EXPECT_FALSE(inst_fail->publish("TEST MESSAGE")) << "Expected failed publish since producer is not created";
    EXPECT_TRUE(inst_fail->stop());
}

TEST_F(GvaMetaPublishKafkaImplFixture, test_produce) {
    ASSERT_TRUE(inst->start());
    auto mock = inst->get_mock_producer();
    EXPECT_CALL(*mock, produce).Times(1).WillOnce(::testing::Return(ErrorCode::ERR_NO_ERROR));
    EXPECT_TRUE(inst->publish("TEST MESSAGE"));
    EXPECT_CALL(*mock, flush).Times(1).WillOnce(::testing::Return(ErrorCode::ERR_NO_ERROR));
    EXPECT_TRUE(inst->stop());
}

TEST_F(GvaMetaPublishKafkaImplFixture, test_produce_fail) {
    ASSERT_TRUE(inst->start());
    auto mock = inst->get_mock_producer();
    EXPECT_CALL(*mock, produce).Times(1).WillOnce(::testing::Return(ErrorCode::ERR__FAIL));
    EXPECT_CALL(*mock, fatal_error).Times(1).WillOnce(::testing::Invoke([](std::string &err) {
        err = "Produce failed by test";
        return ErrorCode::ERR__FAIL;
    }));
    EXPECT_FALSE(inst->publish("TEST MESSAGE")) << "Expected failed 'publish' because 'produce' returns error";
    EXPECT_CALL(*mock, flush).Times(1).WillOnce(::testing::Return(ErrorCode::ERR_NO_ERROR));
    EXPECT_TRUE(inst->stop());
}

TEST_F(GvaMetaPublishKafkaImplFixture, test_flush_fail) {
    ASSERT_TRUE(inst->start()) << "Expected successful start";
    auto mock = inst->get_mock_producer();
    EXPECT_CALL(*mock, flush).Times(1).WillOnce(::testing::Return(ErrorCode::ERR__FAIL));
    EXPECT_CALL(*mock, outq_len).Times(1).WillOnce(::testing::Return(1));
    EXPECT_TRUE(inst->stop());
}

TEST_F(GvaMetaPublishKafkaImplFixture, test_deliver_msg_callback) {
    ASSERT_TRUE(inst->start()) << "Expected successful start";
    auto mock = inst->get_mock_producer();
    MockMessage message;
    EXPECT_CALL(message, err).Times(1).WillOnce(::testing::Return(ErrorCode::ERR_NO_ERROR));
    EXPECT_NO_THROW(mock->dr_msg_cb->dr_cb(message));

    EXPECT_CALL(message, err).Times(1).WillOnce(::testing::Return(ErrorCode::ERR__FAIL));
    EXPECT_NO_THROW(mock->dr_msg_cb->dr_cb(message));
}

TEST_F(GvaMetaPublishKafkaImplFixture, test_error_callback) {
    ASSERT_TRUE(inst->start()) << "Expected successful start";
    auto mock = inst->get_mock_producer();
    MockEvent event;

    EXPECT_CALL(event, type).WillRepeatedly(::testing::Return(Event::EVENT_STATS));
    EXPECT_CALL(event, severity).WillRepeatedly(::testing::Return(Event::EVENT_SEVERITY_DEBUG));
    auto con_attempt = inst->get_connection_attempt();
    EXPECT_NO_THROW(mock->event_cb->event_cb(event));
    EXPECT_EQ(con_attempt, inst->get_connection_attempt())
        << "Expected not changed 'connection_attempt' counter because event is not an error";

    EXPECT_CALL(event, type).WillRepeatedly(::testing::Return(Event::EVENT_LOG));
    EXPECT_CALL(event, severity).WillRepeatedly(::testing::Return(Event::EVENT_SEVERITY_WARNING));
    EXPECT_NO_THROW(mock->event_cb->event_cb(event));
    EXPECT_EQ(con_attempt, inst->get_connection_attempt())
        << "Expected not changed 'connection_attempt' counter because event is not an error";

    EXPECT_CALL(event, type).WillRepeatedly(::testing::Return(Event::EVENT_LOG));
    EXPECT_CALL(event, severity).WillRepeatedly(::testing::Return(Event::EVENT_SEVERITY_ERROR));
    EXPECT_NO_THROW(mock->event_cb->event_cb(event));
    EXPECT_EQ(++con_attempt, inst->get_connection_attempt())
        << "Expected incremented 'connection_attempt' counter because event is an error (LOG with ERROR severity)";

    EXPECT_CALL(event, type).WillRepeatedly(::testing::Return(Event::EVENT_ERROR));
    EXPECT_CALL(event, severity).WillRepeatedly(::testing::Return(Event::EVENT_SEVERITY_DEBUG));
    EXPECT_NO_THROW(mock->event_cb->event_cb(event));
    EXPECT_EQ(++con_attempt, inst->get_connection_attempt())
        << "Expected incremented 'connection_attempt' counter because event is an error";
}

GTEST_API_ int main(int argc, char **argv) {
    std::cout << "Running metapublsh kafka test " << argv[0] << std::endl;
    testing::InitGoogleTest(&argc, argv);
    // Initialize GStreamer
    gst_init(&argc, &argv);
    return RUN_ALL_TESTS();
}
