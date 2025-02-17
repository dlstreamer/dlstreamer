/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/base/transform.h"
#include "dlstreamer/gst/context.h"
#include "dlstreamer/gst/frame.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/utils.h"

#include "batch_create.h"
#include "batch_split.h"
#include "multi_value_storage.h"
#include "shared_instance.h"

enum { PROP_0, PROP_BATCH_SIZE };

constexpr gint MIN_BATCH_SIZE = 0;
constexpr gint MAX_BATCH_SIZE = 1024;
constexpr gint DEFAULT_BATCH_SIZE = 1;

using namespace dlstreamer;

// global storage of transforms working on same shared instance
MultiValueStorage<Element *, GstBaseTransform *> g_gst_base_element_storage;

class BatchCreateImpl : public BaseTransform {
  public:
    BatchCreateImpl(gint batch_size) : BaseTransform(nullptr), _batch_size(batch_size) {
        _buffer_list = gst_buffer_list_new();
        _stream_id_quark = g_quark_from_string(SourceIdentifierMetadata::key::stream_id);
    }

    ~BatchCreateImpl() {
        if (_buffer_list) {
            gst_buffer_list_unref(_buffer_list);
        }
    }

    GstFlowReturn generate_output(GstBuffer *src, intptr_t stream_id, GstPad *pad) {
        GstBufferList *output_list = nullptr;

        { // if shared instance across multiple streams, this function called from multiple threads
            std::lock_guard<std::mutex> guard(_mutex);

            if (src) {
                // Attach stream_id info (transform_wrapper.cpp reads stream_id from qdata to avoid
                // gst_buffer_make_writable)
                gst_mini_object_set_qdata(&src->mini_object, _stream_id_quark, (void *)stream_id, NULL);
                // Append to buffer list. Buffer list takes ownership of the buffer.
                gst_buffer_list_insert(_buffer_list, -1, src);
            }

            // If reached batch_size or in flushing mode, push buffer list downstream and start new buffer list
            if (gst_buffer_list_length(_buffer_list) >= static_cast<guint>(_batch_size) || !src) {
                output_list = _buffer_list;
                _buffer_list = gst_buffer_list_new();
            }
        }

        if (output_list)
            return gst_pad_push_list(pad, output_list);
        else
            return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }

    std::function<FramePtr()> get_output_allocator() override {
        return nullptr;
    }

  private:
    std::mutex _mutex;
    GstBufferList *_buffer_list = nullptr;
    gint _batch_size = 0;
    GQuark _stream_id_quark;
};

struct BatchCreateClass {
    GstBaseTransformClass base_class;
};

struct BatchCreate {
    GstBaseTransform base;
    BatchCreateImpl *impl;
    ElementPtr element; // for ref-counting only
    gint batch_size;
    intptr_t stream_id;
};

// Register GType
#define video_inference_parent_class parent_class
G_DEFINE_TYPE(BatchCreate, batch_create, GST_TYPE_BASE_TRANSFORM);

#define BATCH_CREATE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), batch_create_get_type(), BatchCreate))

static void batch_create_init(BatchCreate *self) {
    self->impl = nullptr;
    self->batch_size = DEFAULT_BATCH_SIZE;
    self->stream_id = 0;
}

static gboolean batch_create_start(GstBaseTransform *base) {
    auto self = BATCH_CREATE(base);
    if (self->impl)
        return TRUE;

    // create shared instance
    BaseDictionary params;
    std::string name = "batch_create";
    std::string shared_instance_id = "auto";
    FrameInfo input_info;
    FrameInfo output_info;
    SharedInstance::InstanceId id = {name, shared_instance_id, params, input_info, output_info};
    auto impl = std::make_shared<BatchCreateImpl>(self->batch_size);
    auto element = SharedInstance::global()->init_or_reuse(id, impl, nullptr);
    self->impl = ptr_cast<BatchCreateImpl>(element).get();
    self->element = element;

    // register instance
    g_gst_base_element_storage.add(element.get(), base);

    // query stream_id
    GSTContextQuery stream_id_ctx(base->srcpad, MemoryType::CPU, STREAMID_CONTEXT_NAME);
    DLS_CHECK(self->stream_id = (intptr_t)stream_id_ctx.handle(STREAMID_CONTEXT_FIELD_NAME));

    return TRUE;
}

static void batch_create_class_init(BatchCreateClass *klass) {
    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = [](GObject *object, guint prop_id, const GValue *value, GParamSpec * /*pspec*/) {
        auto self = BATCH_CREATE(object);
        if (prop_id == PROP_BATCH_SIZE)
            self->batch_size = g_value_get_int(value);
    };
    gobject_class->get_property = [](GObject *object, guint prop_id, GValue *value, GParamSpec * /*pspec*/) {
        auto self = BATCH_CREATE(object);
        if (prop_id == PROP_BATCH_SIZE)
            g_value_set_int(value, self->batch_size);
    };
    gobject_class->finalize = [](GObject *object) {
        auto self = BATCH_CREATE(object);
        if (self->impl)
            g_gst_base_element_storage.remove(self->impl, &self->base);
        self->element.reset();
    };

    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->start = batch_create_start;
    base_transform_class->generate_output = [](GstBaseTransform *base, GstBuffer ** /*outbuf*/) {
        auto self = BATCH_CREATE(base);
        // if shared instance across multiple streams, all streams push to first stream/transform to keep frame order
        GstBaseTransform *first_transform = g_gst_base_element_storage.get_first(self->impl);
        auto ret = self->impl->generate_output(base->queued_buf, self->stream_id, first_transform->srcpad);
        base->queued_buf = NULL;
        return ret;
    };

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, BATCH_CREATE_DESCRIPTION, "application",
                                          BATCH_CREATE_DESCRIPTION, "Intel Corporation");

    gst_element_class_add_pad_template(element_class,
                                       gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_CAPS_ANY));
    gst_element_class_add_pad_template(element_class,
                                       gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_CAPS_ANY));

    g_object_class_install_property(gobject_class, PROP_BATCH_SIZE,
                                    g_param_spec_int("batch-size", "Batch Size", "Number of frames to batch together",
                                                     MIN_BATCH_SIZE, MAX_BATCH_SIZE, DEFAULT_BATCH_SIZE,
                                                     (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}
