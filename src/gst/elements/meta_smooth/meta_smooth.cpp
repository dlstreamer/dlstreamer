/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "meta_smooth.h"

#include "dlstreamer/gst/frame.h"
#include "dlstreamer/gst/utils.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer_logger.h"
#include "gst_logger_sink.h"
#include "metadata.h"
#include "metadata/gva_tensor_meta.h"
#include "region_of_interest.h"
#include <memory>

using namespace dlstreamer;

GST_DEBUG_CATEGORY_STATIC(meta_smooth_debug_category);
#define GST_CAT_DEFAULT meta_smooth_debug_category
struct GstBufferDeleter {
    void operator()(GstBuffer *ptr) noexcept {
        gst_buffer_unref(ptr);
    }
};
using GstBufferReference = std::unique_ptr<GstBuffer, GstBufferDeleter>;

class MetaSmoothPrivate {
  public:
    GstBaseTransform *_base;
    std::map<int, GstBufferReference> _objectId_to_buffer;
    std::shared_ptr<spdlog::logger> _logger;

  public:
    gboolean sink_event(GstEvent *event);
    GstFlowReturn meta_smooth_transform_ip(GstBuffer *buf);

  private:
    void restore_roi_id(GstBuffer *meta_buffer, int roi_id);
    GstBufferReference copy_buffer(GstBuffer *input);
};
G_DEFINE_TYPE_WITH_PRIVATE(MetaSmooth, meta_smooth, GST_TYPE_BASE_TRANSFORM);

void MetaSmoothPrivate::restore_roi_id(GstBuffer *meta_buffer, int roi_id) {
    GstGVATensorMeta *custom_meta;
    gpointer state = nullptr;
    while ((custom_meta = GST_GVA_TENSOR_META_ITERATE(meta_buffer, &state))) {
        std::string name = g_quark_to_string(custom_meta->data->name);
        if (name != SourceIdentifierMetadata::name)
            continue;
        GValue gelem = G_VALUE_INIT;
        g_value_init(&gelem, G_TYPE_INT);
        g_value_set_int(&gelem, roi_id);
        gst_structure_set_value(custom_meta->data, SourceIdentifierMetadata::key::roi_id, &gelem);
    }
}

GstBufferReference MetaSmoothPrivate::copy_buffer(GstBuffer *input_buffer) {
    GstBufferReference output_buffer = GstBufferReference(gst_buffer_new());
    auto copy_flags = (GstBufferCopyFlags)(GST_BUFFER_COPY_TIMESTAMPS | GST_BUFFER_COPY_META | GST_BUFFER_COPY_MEMORY);
    if (!gst_buffer_copy_into(output_buffer.get(), input_buffer, copy_flags, 0, static_cast<gsize>(-1)))
        return nullptr;
    return output_buffer;
}

gboolean MetaSmoothPrivate::sink_event(GstEvent *event) {
    if (event->type != GST_EVENT_GAP)
        return GST_BASE_TRANSFORM_CLASS(meta_smooth_parent_class)->sink_event(_base, event);
    const GstStructure *event_structure = gst_event_get_structure(event);

    int object_id = 0;
    if (!gst_structure_get_int(event_structure, SourceIdentifierMetadata::key::object_id, &object_id))
        return GST_BASE_TRANSFORM_CLASS(meta_smooth_parent_class)->sink_event(_base, event);

    auto buffer_iter = _objectId_to_buffer.find(object_id);
    if (buffer_iter == _objectId_to_buffer.end()) {
        SPDLOG_LOGGER_WARN(_logger, "object id: {} missed in storage", object_id);
        return GST_BASE_TRANSFORM_CLASS(meta_smooth_parent_class)->sink_event(_base, event);
    }

    int roi_id = 0;
    if (!gst_structure_get_int(event_structure, SourceIdentifierMetadata::key::roi_id, &roi_id))
        return GST_BASE_TRANSFORM_CLASS(meta_smooth_parent_class)->sink_event(_base, event);
    const GValue *gvalue_pts = gst_structure_get_value(event_structure, SourceIdentifierMetadata::key::pts);

    GstBufferReference output_buffer = copy_buffer(buffer_iter->second.get());
    if (!output_buffer) {
        SPDLOG_LOGGER_ERROR(_logger, "Failed to copy data from cache buffer into output buffer");
        return GST_FLOW_ERROR;
    }
    output_buffer->pts = reinterpret_cast<GstClockTime>(g_value_get_pointer(gvalue_pts));
    restore_roi_id(output_buffer.get(), roi_id);

    SPDLOG_LOGGER_DEBUG(_logger, "push buffer: {} object_id: {} roi_id: {} cur_pts: {} on srcpad ",
                        fmt::ptr(output_buffer.get()), object_id, roi_id, output_buffer->pts);
    gst_buffer_ref(output_buffer.get()); // we lose reference to buffer after calling gst_pad_push, so need to keep it.
    return gst_pad_push(_base->srcpad, output_buffer.get());
}

GstFlowReturn MetaSmoothPrivate::meta_smooth_transform_ip(GstBuffer *buf) {
    SPDLOG_LOGGER_DEBUG(_logger, "meta_smooth_transform_ip {}", fmt::ptr(buf));
    // extract object id
    GSTMetadata meta_wrapper(buf);
    auto si_meta = SourceIdentifierMetadata::try_cast(meta_wrapper.find_metadata(SourceIdentifierMetadata::name));
    if (!si_meta) {
        SPDLOG_LOGGER_WARN(_logger, "no SourceIdentifierMetadata");
        return GST_FLOW_OK;
    }
    std::optional<Any> object_id_any = si_meta->try_get(SourceIdentifierMetadata::key::object_id);
    if (!object_id_any) {
        SPDLOG_LOGGER_WARN(_logger, "missed object id in SourceIdentifierMetadata");
        return GST_FLOW_OK;
    }
    int object_id = any_cast<int>(*object_id_any);

    GstBufferReference cache_buffer = copy_buffer(buf);
    if (!cache_buffer) {
        SPDLOG_LOGGER_ERROR(_logger, "Failed to copy data from input buffer into cache buffer");
        return GST_FLOW_ERROR;
    }

    SPDLOG_LOGGER_DEBUG(_logger, "save metadata object_id: {} cached_buffer: {} orig_buffer: {}", object_id,
                        fmt::ptr(cache_buffer.get()), fmt::ptr(buf));
    _objectId_to_buffer[object_id] = std::move(cache_buffer);
    return GST_FLOW_OK;
}

static void meta_smooth_init(MetaSmooth *self) {
    auto *priv_memory = meta_smooth_get_instance_private(self);
    // This won't be converted to shared ptr because of memory placement
    self->impl = new (priv_memory) MetaSmoothPrivate();
    self->impl->_base = &self->base;
    self->impl->_logger = log::init_logger(meta_smooth_debug_category, G_OBJECT(self));
    SPDLOG_LOGGER_INFO(self->impl->_logger, "");
}

static void meta_smooth_finalize(GObject *object) {
    MetaSmooth *self = META_SMOOTH(object);
    SPDLOG_LOGGER_INFO(self->impl->_logger, "");
    if (self->impl) {
        self->impl->~MetaSmoothPrivate();
        self->impl = nullptr;
    }
    G_OBJECT_CLASS(meta_smooth_parent_class)->finalize(object);
}

static void meta_smooth_class_init(MetaSmoothClass *klass) {
    GST_DEBUG_CATEGORY_INIT(meta_smooth_debug_category, "gva_meta_smooth", 0, "debug category for meta_smooth");

    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = meta_smooth_finalize;

    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);

    base_transform_class->sink_event = [](GstBaseTransform *base, GstEvent *event) {
        return META_SMOOTH(base)->impl->sink_event(event);
    };
    base_transform_class->transform_ip = [](GstBaseTransform *base, GstBuffer *buf) {
        return META_SMOOTH(base)->impl->meta_smooth_transform_ip(buf);
    };

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, META_SMOOTH_NAME, "Filter/Metadata", META_SMOOTH_DESCRIPTION,
                                          "Intel Corporation");

    gst_element_class_add_pad_template(element_class,
                                       gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_CAPS_ANY));
    gst_element_class_add_pad_template(element_class,
                                       gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_CAPS_ANY));
}
