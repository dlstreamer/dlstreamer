/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "batch_split.h"
#include "dlstreamer/gst/frame.h"
#include "dlstreamer/image_metadata.h"

using namespace dlstreamer;

GST_DEBUG_CATEGORY_STATIC(batch_split_debug_category);
#define GST_CAT_DEFAULT batch_split_debug_category

G_DEFINE_TYPE(TensorSplitBatch, batch_split, GST_TYPE_BASE_TRANSFORM);

enum { PROP_0 };

static void batch_split_init(TensorSplitBatch *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);
}

static void batch_split_set_property(GObject *object, guint prop_id, const GValue * /*value*/, GParamSpec *pspec) {
    TensorSplitBatch *self = TENSOR_SPLIT_BATCH(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void batch_split_get_property(GObject *object, guint prop_id, GValue * /*value*/, GParamSpec *pspec) {
    TensorSplitBatch *self = TENSOR_SPLIT_BATCH(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void batch_split_dispose(GObject *object) {
    TensorSplitBatch *self = TENSOR_SPLIT_BATCH(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    G_OBJECT_CLASS(batch_split_parent_class)->dispose(object);
}

static void batch_split_finalize(GObject *object) {
    TensorSplitBatch *self = TENSOR_SPLIT_BATCH(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    G_OBJECT_CLASS(batch_split_parent_class)->finalize(object);
}

gboolean batch_split_query(GstBaseTransform *base, GstPadDirection direction, GstQuery *query) {
    GST_DEBUG_OBJECT(base, "query");

    if (GST_QUERY_TYPE(query) == GST_QUERY_CONTEXT) {
        const gchar *context_type;
        gst_query_parse_context_type(query, &context_type);
        if (context_type && std::string(context_type) == STREAMID_CONTEXT_NAME) {
            auto gst_ctx = gst_context_new(context_type, FALSE);
            auto s = gst_context_writable_structure(gst_ctx);
            gst_structure_set(s, STREAMID_CONTEXT_FIELD_NAME, G_TYPE_POINTER, base, NULL);
            gst_query_set_context(query, gst_ctx);
            gst_context_unref(gst_ctx);
            GST_LOG_OBJECT(base, "Created context of type %s", context_type);
            return TRUE;
        }
    }

    return GST_BASE_TRANSFORM_CLASS(batch_split_parent_class)->query(base, direction, query);
}

static gboolean batch_split_start(GstBaseTransform *base) {
    TensorSplitBatch *self = TENSOR_SPLIT_BATCH(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return true;
}

static gboolean batch_split_stop(GstBaseTransform *base) {
    TensorSplitBatch *self = TENSOR_SPLIT_BATCH(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return true;
}

static GstFlowReturn batch_split_transform_ip(GstBaseTransform *base, GstBuffer *buf) {
    TensorSplitBatch *self = TENSOR_SPLIT_BATCH(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    FrameInfo info; // TODO
    GSTFrame buffer(buf, info);
    for (auto &meta : buffer.metadata()) {
        if (meta->name() != SourceIdentifierMetadata::name)
            continue;

        // Create new buffer (without GstMemory copy)
        GstBuffer *dst_buff = gst_buffer_copy(buf);

        // Set PTS
        GST_BUFFER_PTS(dst_buff) = meta->get<intptr_t>(SourceIdentifierMetadata::key::pts);

        // Metadata
        {
            GSTFrame dst_buff_dls(dst_buff, info);
            auto &metadata = dst_buff_dls.metadata();
            // Remove all SourceIdentifierMetadata
            metadata.erase(std::remove_if(metadata.begin(), metadata.end(),
                                          [](const DictionaryPtr &dict) {
                                              return dict->name() == SourceIdentifierMetadata::name;
                                          }),
                           metadata.end());

            // Add only one SourceIdentifierMetadata
            auto dst_meta = metadata.add(SourceIdentifierMetadata::name);
            for (auto &key : meta->keys()) {
                dst_meta->set(key, *meta->try_get(key));
            }
        }

        // Call pad_push on corresponding transform
        auto stream_id = meta->get<intptr_t>(SourceIdentifierMetadata::key::stream_id);
        GstBaseTransform *tran = reinterpret_cast<GstBaseTransform *>(stream_id); // stream_id = GstBaseTransform*
        if (!tran) {
            GST_ERROR_OBJECT(base, "stream_id not specified");
            throw std::runtime_error("No stream_id in SourceIdentifierMetadata");
        }
        GstFlowReturn ret = gst_pad_push(GST_BASE_TRANSFORM_SRC_PAD(tran), dst_buff);
        if (ret != GST_FLOW_OK) {
            GST_ERROR_OBJECT(base, "Failed to push buffer");
            return ret;
        }
    }

    return GST_BASE_TRANSFORM_FLOW_DROPPED;
}

static void batch_split_class_init(TensorSplitBatchClass *klass) {
    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = batch_split_set_property;
    gobject_class->get_property = batch_split_get_property;
    gobject_class->dispose = batch_split_dispose;
    gobject_class->finalize = batch_split_finalize;

    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->query = batch_split_query;
    base_transform_class->start = batch_split_start;
    base_transform_class->stop = batch_split_stop;
    base_transform_class->transform_ip = batch_split_transform_ip;

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, TENSOR_SPLIT_BATCH_NAME, "application",
                                          TENSOR_SPLIT_BATCH_DESCRIPTION, "Intel Corporation");

    gst_element_class_add_pad_template(element_class,
                                       gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_CAPS_ANY));
    gst_element_class_add_pad_template(element_class,
                                       gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_CAPS_ANY));
}
