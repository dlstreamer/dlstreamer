/*******************************************************************************
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <functional>
#include <gtest/gtest.h>

#include <gst/gst.h>

inline std::ostream &operator<<(std::ostream &os, const GstElement &element) {
    return os << GST_ELEMENT_NAME(&element);
}

inline std::ostream &operator<<(std::ostream &os, const GstStaticCaps &scaps) {
    return os << scaps.string;
}

inline std::ostream &operator<<(std::ostream &os, const GstCaps &caps) {
    auto str = gst_caps_to_string(&caps);
    os << str;
    g_free(str);
    return os;
}

class ElementTest : public ::testing::Test {
  protected:
    std::string _elementName;
    GstStaticCaps _srcCaps;
    GstStaticCaps _sinkCaps;

    // FIXME: Probably, should extracted as separate utility class
    struct TestPad {
        GstPad *pad = nullptr;

        using PadQueryCallback = std::function<bool(GstPad *, GstObject *, GstQuery *)>;
        using PadEventCallback = std::function<bool(GstPad *, GstObject *, GstEvent *)>;
        PadQueryCallback queryFn;
        PadEventCallback eventFn;

        void setQueryCallback(PadQueryCallback cb) {
            ASSERT_FALSE(queryFn) << "Query callback already has been set. Overriding is not yet supported.";

            queryFn = cb;
            auto fn = [](GstPad *pad, GstObject *parent, GstQuery *query) -> gboolean {
                auto self = static_cast<TestPad *>(pad->querydata);
                assert(pad == self->pad);
                return self->queryFn(pad, parent, query);
            };
            gst_pad_set_query_function_full(pad, fn, this, nullptr);
        }

        void setEventCallback(PadEventCallback cb) {
            ASSERT_FALSE(eventFn) << "Event callback already has been set. Overriding is not yet supported.";

            eventFn = cb;
            auto fn = [](GstPad *pad, GstObject *parent, GstEvent *query) -> gboolean {
                auto self = static_cast<TestPad *>(pad->eventdata);
                assert(pad == self->pad);
                return self->eventFn(pad, parent, query);
            };
            gst_pad_set_event_function_full(pad, fn, this, nullptr);
        }

        void teardown() {
            GstPad *peer = gst_pad_get_peer(pad);
            if (peer) {
                if (GST_PAD_DIRECTION(pad) == GST_PAD_SRC)
                    gst_pad_unlink(pad, peer);
                else
                    gst_pad_unlink(peer, pad);
                gst_object_unref(peer);
            }
            gst_object_unref(pad);

            pad = nullptr;
        }
    };

    TestPad _testSinkPad;
    TestPad _testSrcPad;

    GstElement *_element = nullptr;
    GstBus *_bus = nullptr;

    virtual GstFlowReturn sink_pad_chain(GstPad * /*pad*/, GstObject * /*parent*/, GstBuffer *buffer) {
        gst_buffer_unref(buffer);
        return GST_FLOW_OK;
    }

    bool defaultQueryHandler(GstPad *pad, GstObject *parent, GstQuery *query) {
        return gst_pad_query_default(pad, parent, query);
    }

    bool defaultEventHandler(GstPad *pad, GstObject *parent, GstEvent *event) {
        return gst_pad_event_default(pad, parent, event);
    }

    virtual bool sink_pad_query(GstPad *pad, GstObject *parent, GstQuery *query) {
        return defaultQueryHandler(pad, parent, query);
    }

    virtual bool sink_pad_event(GstPad *pad, GstObject *parent, GstEvent *event) {
        return defaultEventHandler(pad, parent, event);
    }

    virtual bool test_src_pad_query(GstPad *pad, GstObject *parent, GstQuery *query) {
        return defaultQueryHandler(pad, parent, query);
    }

    virtual bool test_src_pad_event(GstPad *pad, GstObject *parent, GstEvent *event) {
        return defaultEventHandler(pad, parent, event);
    }

    void createElement() {
        SCOPED_TRACE("Element name: " + _elementName);
        _element = gst_element_factory_make(_elementName.c_str(), _elementName.c_str());
        ASSERT_NE(_element, nullptr);
    }

    void createTestPads() {
        GstStaticPadTemplate srcTempl = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, _srcCaps);
        _testSrcPad.pad = gst_pad_new_from_static_template(&srcTempl, "src");
        ASSERT_NE(_testSrcPad.pad, nullptr) << "Couldn't create test src pad with caps: " << _srcCaps;
        ASSERT_TRUE(gst_pad_set_active(_testSrcPad.pad, true));

        GstStaticPadTemplate sinkTempl = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, _sinkCaps);
        _testSinkPad.pad = gst_pad_new_from_static_template(&sinkTempl, "sink");
        ASSERT_NE(_testSinkPad.pad, nullptr) << "Couldn't create test sink pad with caps: " << _srcCaps;
        ASSERT_TRUE(gst_pad_set_active(_testSinkPad.pad, true));
    }

    void linkTestPads() {
        setupTestPadsCallbacks();

        {
            GstPad *sinkPad = gst_element_get_static_pad(_element, "sink");
            ASSERT_NE(sinkPad, nullptr) << "Couldn't get sink pad of element '" << *_element << '\'';
            ASSERT_EQ(gst_pad_link(_testSrcPad.pad, sinkPad), GST_PAD_LINK_OK)
                << "Couldn't link test source pad with sink pad of element '" << *_element << '\'';
        }

        {
            GstPad *srcPad = gst_element_get_static_pad(_element, "src");
            ASSERT_NE(srcPad, nullptr) << "Couldn't get src pad of element '" << *_element << '\'';
            ASSERT_EQ(gst_pad_link(srcPad, _testSinkPad.pad), GST_PAD_LINK_OK)
                << "Couldn't link test source pad of element '" << *_element << "' with test sink";
        }
    }

    void setupPadEvents(GstPad *pad, GstCaps *caps) {
        // Stream start
        auto sid = gst_pad_create_stream_id(pad, _element, nullptr);
        ASSERT_TRUE(gst_pad_push_event(pad, gst_event_new_stream_start(sid)))
            << "Couldn't push stream start event with ID: " << sid;
        g_free(sid);

        // Caps
        if (!gst_caps_is_fixed(caps))
            caps = gst_caps_fixate(caps);
        ASSERT_TRUE(gst_pad_push_event(pad, gst_event_new_caps(caps)));

        // New segment
        GstSegment segment;
        gst_segment_init(&segment, GST_FORMAT_BYTES);
        ASSERT_TRUE(gst_pad_push_event(pad, gst_event_new_segment(&segment)));
    }

    void setupBus() {
        _bus = gst_bus_new();
    }

    void setupElement() {
        createElement();
        gst_element_set_bus(_element, _bus);
        createTestPads();
    }

    void setupEvents() {
        linkTestPads();
        setupPadEvents(_testSrcPad.pad, gst_static_caps_get(&_srcCaps));
    }

    void SetUp() override {
        setupBus();
        setupElement();
        // TODO: set state here?
        setupEvents();
    }

    void tearDownTestPads() {
        _testSrcPad.teardown();
        _testSinkPad.teardown();
    }

    void TearDown() override {
        SCOPED_TRACE("Element name: " + _elementName);
        ASSERT_TRUE(setState(GST_STATE_NULL)) << "Couldn't set state to NULL";

        tearDownTestPads();

        if (_bus)
            gst_bus_set_flushing(_bus, true);

        const auto refcount = GST_OBJECT_REFCOUNT_VALUE(_element);
        EXPECT_EQ(refcount, 1) << "Reference count of element should be 1 at teardown";
        gst_object_unref(_element);

        if (_bus)
            gst_object_unref(_bus);
    }

    bool hasErrorOnBus(std::string &error) {
        GstMessage *message = gst_bus_pop_filtered(_bus, GST_MESSAGE_ERROR);
        if (message) {
            gchar *debug = nullptr;
            gst_message_parse_error(message, nullptr, &debug);
            error = debug;
            g_free(debug);
            gst_message_unref(message);
            return true;
        }
        return false;
    }

    bool hasErrorOnBus() {
        std::string tmp;
        return hasErrorOnBus(tmp);
    }

    void setSrcCaps(GstCaps *caps) {
        SCOPED_TRACE("Pushing CAPS event");
        ASSERT_TRUE(gst_caps_is_fixed(caps)) << "Caps must be fixed. Caps are: " << *caps;
        ASSERT_TRUE(gst_pad_push_event(_testSrcPad.pad, gst_event_new_caps(caps)))
            << "Couldn't push caps event: " << *caps;
        GstSegment segment;
        gst_segment_init(&segment, GST_FORMAT_TIME);
        ASSERT_TRUE(gst_pad_push_event(_testSrcPad.pad, gst_event_new_segment(&segment)));
    }

    GParamSpec *findProperty(const char *name) const {
        return g_object_class_find_property(G_OBJECT_GET_CLASS(_element), name);
    }

    bool hasProperty(const char *name) const {
        return findProperty(name) != nullptr;
    }

    void getProperty(const char *name, GValue *value) const {
        SCOPED_TRACE(::testing::Message() << "getProperty('" << name << "')");
        auto *paramSpec = findProperty(name);
        ASSERT_NE(paramSpec, nullptr) << "Element '" << *_element << "' has no property '" << name << '\'';

        g_value_init(value, paramSpec->value_type);
        g_object_get_property(G_OBJECT(_element), name, value);
    }

    void setProperty(const char *name, const GValue &value) {
        ASSERT_TRUE(hasProperty(name)) << "Element '" << *_element << "' has no property '" << name << '\'';
        g_object_set_property(G_OBJECT(_element), name, &value);
    }

    void setProperty(const char *name, const std::string &value) {
        SCOPED_TRACE(::testing::Message() << "setProperty('" << name << "', " << value << ')');
        GValue gval = G_VALUE_INIT;
        g_value_init(&gval, G_TYPE_STRING);
        g_value_set_string(&gval, value.c_str());
        setProperty(name, gval);
    }

    void setProperty(const char *name, int value) {
        SCOPED_TRACE(::testing::Message() << "setProperty('" << name << "', " << value << ')');
        GValue gval = G_VALUE_INIT;
        g_value_init(&gval, G_TYPE_INT);
        g_value_set_int(&gval, value);
        setProperty(name, gval);
    }

    GstState getState() const {
        GstState state;
        gst_element_get_state(_element, &state, nullptr, GST_CLOCK_TIME_NONE);
        return state;
    }

    bool setState(GstState state, bool treatNoPrerollStsAsSuccess = true) {
        GstStateChangeReturn ret = gst_element_set_state(_element, state);
        if (ret == GST_STATE_CHANGE_ASYNC)
            ret = gst_element_get_state(_element, nullptr, nullptr, GST_CLOCK_TIME_NONE);
        return ret == GST_STATE_CHANGE_SUCCESS || (treatNoPrerollStsAsSuccess && ret == GST_STATE_CHANGE_NO_PREROLL);
    }

    GstBuffer *createRandomBuffer(size_t bytes_size) {
        auto buffer = gst_buffer_new_and_alloc(bytes_size);
        GRand *rand = g_rand_new_with_seed(bytes_size);
        GstMapInfo info;
        EXPECT_TRUE(gst_buffer_map(buffer, &info, GST_MAP_WRITE));
        for (auto i = 0u; i < bytes_size; ++i)
            ((guint8 *)info.data)[i] = (g_rand_int(rand) >> 24) & 0xff;
        gst_buffer_unmap(buffer, &info);
        GST_BUFFER_TIMESTAMP(buffer) = 0;
        return buffer;
    }

    bool pushBuffer(GstBuffer *buffer) {
        return gst_pad_push(_testSrcPad.pad, buffer) == GST_FLOW_OK;
    }

    uint32_t countMeta(GstBuffer *buffer) {
        GstMeta *meta = nullptr;
        gpointer state = nullptr;
        uint32_t count = 0;
        while ((meta = gst_buffer_iterate_meta(buffer, &state)))
            count++;
        return count;
    }

  private:
    TestPad &findTestPad(GstPad *pad) {
        if (pad == _testSinkPad.pad)
            return _testSinkPad;
        if (pad == _testSrcPad.pad)
            return _testSrcPad;
        throw std::runtime_error("Couldn't find corresponding test pad");
    }

    void setupTestPadsCallbacks() {
        gst_pad_set_chain_function_full(
            _testSinkPad.pad,
            [](GstPad *pad, GstObject *parent, GstBuffer *buffer) {
                return static_cast<ElementTest *>(pad->chaindata)->sink_pad_chain(pad, parent, buffer);
            },
            this, nullptr);

        using std::placeholders::_1;
        using std::placeholders::_2;
        using std::placeholders::_3;

        _testSinkPad.setQueryCallback(std::bind(&ElementTest::sink_pad_query, this, _1, _2, _3));
        _testSinkPad.setEventCallback(std::bind(&ElementTest::sink_pad_event, this, _1, _2, _3));
        _testSrcPad.setQueryCallback(std::bind(&ElementTest::test_src_pad_query, this, _1, _2, _3));
        _testSrcPad.setEventCallback(std::bind(&ElementTest::test_src_pad_event, this, _1, _2, _3));
    }
};
