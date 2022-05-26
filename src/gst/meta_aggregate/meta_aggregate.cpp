/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "meta_aggregate.h"
#include "dlstreamer/gst/buffer.h"
#include "dlstreamer/metadata.h"
// FIXME: switch completely to SourceIdentifierMetadata and remove this
#include "gva_roi_ref_meta.hpp"
#include "gva_tensor_meta.h"
#include "meta/gva_buffer_flags.hpp" // TODO move GVA_BUFFER_FLAG_LAST_ROI_ON_FRAME and remove this include
#include <list>

GST_DEBUG_CATEGORY(meta_aggregate_debug);
#define GST_CAT_DEFAULT meta_aggregate_debug

G_DEFINE_TYPE(MetaAggregatePad, meta_aggregate_pad, GST_TYPE_AGGREGATOR_PAD);

static void meta_aggregate_pad_class_init(MetaAggregatePadClass *) {
}

static void meta_aggregate_pad_init(MetaAggregatePad *) {
}

// ----

namespace {
GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

GstStaticPadTemplate sink_templ = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

GstStaticPadTemplate tensor_templ =
    GST_STATIC_PAD_TEMPLATE("tensor_%u", GST_PAD_SINK, GST_PAD_REQUEST, GST_STATIC_CAPS(DLS_TENSOR_MEDIA_NAME "(ANY)"));
} // namespace

class MetaAggregatePrivate {
  public:
    MetaAggregatePrivate(GstAggregator *parent) : mybase_(parent) {
        roi_ref_meta_api_ = g_type_from_name(GVA_ROI_REF_META_API_NAME);
    }

    ~MetaAggregatePrivate() {
        // TODO
        if (current_caps_)
            gst_caps_unref(current_caps_);
    }

    // Returns first sink pad
    MetaAggregatePad *firstSink() const {
#if 0
        GList *pads_list = GST_ELEMENT(mybase_)->sinkpads;
        return reinterpret_cast<MetaAggregatePad *>(pads_list->data);
#else
        return reinterpret_cast<MetaAggregatePad *>(gst_element_get_static_pad(GST_ELEMENT_CAST(mybase_), "sink"));
#endif
    }

    gboolean sinkEvent(GstAggregatorPad *pad, GstEvent *event);
    gboolean srcQuery(GstQuery *query);
    gboolean sinkQuery(GstAggregatorPad *pad, GstQuery *query);

    GstFlowReturn updateSrcCaps(GstCaps *caps, GstCaps **ret) {
        if (!current_caps_)
            return GST_AGGREGATOR_FLOW_NEED_DATA;
        if (!gst_caps_can_intersect(current_caps_, caps))
            return GST_FLOW_NOT_NEGOTIATED;
        *ret = gst_caps_intersect(caps, current_caps_);
        return GST_FLOW_OK;
    }

    GstAggregatorPad *createNewPad(GstPadTemplate *templ, const gchar *, const GstCaps *) {
        if (templ->direction != GST_PAD_SINK) {
            GST_WARNING_OBJECT(mybase_, "Requested new pad that is not SINK pad");
            return nullptr;
        }

        if (templ->presence != GST_PAD_REQUEST) {
            GST_WARNING_OBJECT(mybase_, "Requested new pad that is not REQUEST pad");
            return nullptr;
        }

        if (!g_str_has_prefix(templ->name_template, "tensor_"))
            return nullptr;

        GST_OBJECT_LOCK(mybase_);
        gchar *name = g_strdup_printf("tensor_%u", tensor_pad_num_++);
        auto *res_pad = reinterpret_cast<MetaAggregatePad *>(g_object_new(
            GST_TYPE_META_AGGREGATE_PAD, "name", name, "direction", GST_PAD_SINK, "template", templ, NULL));
        g_free(name);
        GST_OBJECT_UNLOCK(mybase_);

        return &res_pad->parent;
    }

    GstFlowReturn aggregate(bool timeout) {
        // ITT_TASK(GST_ELEMENT_NAME(mybase_));
        // TODO: thread safety ???

        if (!current_buf_) {
            auto ret = pickNextCurrentBuf_();
            if (ret != GST_FLOW_OK) {
                // TODO: handle EOS properly
                return ret;
            }
        }

        auto ret = gatherMeta_(timeout);
        if (ret != GST_FLOW_OK) {
            return ret;
        }

        ret = finishCurrentBuffer_();
        return ret;
    }

  private:
    GstFlowReturn finishCurrentBuffer_() {
        // ITT_TASK("Finish buffer");
        auto first_pad = firstSink();
        gst_aggregator_pad_drop_buffer(&first_pad->parent);
        GST_DEBUG_OBJECT(mybase_, "Finish current buffer: ts=%" GST_TIME_FORMAT,
                         GST_TIME_ARGS(GST_BUFFER_PTS(current_buf_)));
        auto ret = gst_aggregator_finish_buffer(mybase_, current_buf_);
        // TODO: use gst_buffer_replace ???
        current_buf_ = nullptr;
        current_running_time_ = current_running_time_end_ = GST_CLOCK_TIME_NONE;

        return ret;
    }

    GstFlowReturn pickNextCurrentBuf_() {
        // The current buffer must not be set
        g_assert(!current_buf_);

        auto *first_pad = firstSink();

        GstBuffer *buf = gst_aggregator_pad_peek_buffer(&first_pad->parent);
        if (!buf) {
            if (gst_aggregator_pad_is_eos(&first_pad->parent)) {
                GST_DEBUG_OBJECT(mybase_, "EOS on first pad, we're done");
                return GST_FLOW_EOS;
            }

            // TODO: Can be reached ???
            return GST_AGGREGATOR_FLOW_NEED_DATA;
        }

        auto deleter = [](GstBuffer *ptr) { gst_buffer_unref(ptr); };
        auto sg_buf = std::unique_ptr<GstBuffer, decltype(deleter)>(buf, deleter);

        GstClockTime time_start = GST_BUFFER_PTS(buf);
        if (!GST_CLOCK_TIME_IS_VALID(time_start)) {
            GST_ERROR_OBJECT(mybase_, "Video buffer without PTS");
            return GST_FLOW_ERROR;
        }

        time_start = gst_segment_to_running_time(&first_pad->parent.segment, GST_FORMAT_TIME, time_start);
        if (!GST_CLOCK_TIME_IS_VALID(time_start)) {
            GST_DEBUG_OBJECT(mybase_, "Buffer outside segment, dropping");
            gst_aggregator_pad_drop_buffer(&first_pad->parent);
            return GST_AGGREGATOR_FLOW_NEED_DATA;
        }

        sg_buf.release();

        GstClockTime buf_duration;
        if (!GST_BUFFER_DURATION_IS_VALID(buf)) {
            // Need to set the duration to some value because in case of single image
            // processing the duration is set to NONE and pipeline doesn't work
            GST_WARNING_OBJECT(mybase_, "Buffer has invalid duration, using default = 1 nanosecond");
            buf_duration = GST_NSECOND;
        } else {
            buf_duration = GST_BUFFER_DURATION(buf);
        }

        GstClockTime end_time = GST_BUFFER_PTS(buf) + buf_duration;
        g_assert(end_time <= first_pad->parent.segment.stop);

        current_buf_ = buf;
        current_running_time_ = time_start;
        current_running_time_end_ = gst_segment_to_running_time(&first_pad->parent.segment, GST_FORMAT_TIME, end_time);
        GST_DEBUG_OBJECT(mybase_, "Selected current buffer %p, running time: %" GST_TIME_FORMAT " -> %" GST_TIME_FORMAT,
                         current_buf_, GST_TIME_ARGS(current_running_time_), GST_TIME_ARGS(current_running_time_end_));

        return GST_FLOW_OK;
    }

    GstFlowReturn gatherMeta_(bool timeout) {
        // The current buffer must be set
        g_assert(current_buf_);
        g_assert(current_running_time_ != GST_CLOCK_TIME_NONE);
        g_assert(current_running_time_end_ != GST_CLOCK_TIME_NONE);

        bool need_more_data = false;
        // NB: Skipping first sink here.
        for (GList *it = GST_ELEMENT(mybase_)->sinkpads->next; it; it = it->next) {
            auto *pad = reinterpret_cast<MetaAggregatePad *>(it->data);

            auto ret = gatherMetaFromPad_(pad, timeout);
            if (ret != GST_FLOW_OK) {
                if (ret == GST_AGGREGATOR_FLOW_NEED_DATA) {
                    need_more_data = true;
                } else {
                    GST_ERROR_OBJECT(mybase_, "Error occurred while gathering buffers on pad %" GST_PTR_FORMAT, pad);
                    return ret;
                }
            }
        }

        if (need_more_data)
            return GST_AGGREGATOR_FLOW_NEED_DATA;

        // Gathered everything from all pads. Let's merge!
        mergeMetadata();

        return GST_FLOW_OK;
    }

    GstFlowReturn gatherMetaFromPad_(MetaAggregatePad *pad, bool timeout) {
        while (true) {
            GstBuffer *buf = gst_aggregator_pad_peek_buffer(&pad->parent);
            if (!buf) {
                if (gst_aggregator_pad_is_eos(&pad->parent)) {
                    GST_DEBUG_OBJECT(mybase_, "Got EOS on pad %" GST_PTR_FORMAT, pad);
                    break;
                }

                if (!timeout) {
                    GST_DEBUG_OBJECT(mybase_, "Wating for more data on pad %" GST_PTR_FORMAT, pad);
                    return GST_AGGREGATOR_FLOW_NEED_DATA;
                } else {
                    GST_DEBUG_OBJECT(mybase_, "No data on timeout on pad %" GST_PTR_FORMAT, pad);
                    break;
                }
            }

            GstClockTime buf_time = GST_BUFFER_PTS(buf);
            if (!GST_CLOCK_TIME_IS_VALID(buf_time)) {
                GST_ERROR_OBJECT(mybase_, "Got buffer without PTS on pad %" GST_PTR_FORMAT, pad);

                gst_buffer_unref(buf);
                return GST_FLOW_ERROR;
            }

            buf_time = gst_segment_to_running_time(&pad->parent.segment, GST_FORMAT_TIME, buf_time);
            if (!GST_CLOCK_TIME_IS_VALID(buf_time)) {
                GST_DEBUG_OBJECT(mybase_, "Buffer %" GST_PTR_FORMAT " outside segment -> dropping", buf);
                gst_aggregator_pad_drop_buffer(&pad->parent);
                gst_buffer_unref(buf);

                continue;
            }

            if (gst_buffer_has_flags(buf, GST_BUFFER_FLAG_GAP)) {
                GST_DEBUG_OBJECT(mybase_, "Buffer %" GST_PTR_FORMAT " with GAP -> dropping", buf);
                gst_aggregator_pad_drop_buffer(&pad->parent);
                gst_buffer_unref(buf);

                // TODO: check buffer time + duration ???
                break;
            }

            /**
             * Previous condition was:
             * if (buf_time >= current_running_time_end_) {
             *
             * With some decoders we see some small deviation in timestamps. Specifically,
             * Timestamp + duration of buffer is more than timestamp of next buffer
             * So situations are possible when current buffer (on main video sink pad) has
             * current_running_time_ = 83416667
             * current_running_time_end_ = 125125001 (ts + dur)
             * And buffers on tensor pads have following TSs:
             * 1st buffer = 83416667
             * 2nd buffer = 125125000
             * As you can see second buffer should belong to the **next** buffer on main sink pad.
             * But due to some small deviation, we think that 2nd buffer corresponds to current main buffer.
             * Requires further investigation.
             *
             * In general, since we don't process different video streams, and don't change the timestamps of original
             * buffers, we can assume that current buffer ends where it started. And it should work flawlessly.
             */
            if (buf_time > current_running_time_) {
                // This's upcoming buffer, so everything is gathered for current one at this point
                gst_buffer_unref(buf);
                break;
            }

            GST_DEBUG_OBJECT(mybase_, "Collecting metadata buffer %p %" GST_TIME_FORMAT " for current buffer %p", buf,
                             GST_TIME_ARGS(buf_time), current_buf_);

            gst_aggregator_pad_drop_buffer(&pad->parent);

            current_meta_bufs_.push_back(buf);

            // Early exit if we found our custom flag
            if (gst_buffer_has_flags(buf, static_cast<GstBufferFlags>(GVA_BUFFER_FLAG_LAST_ROI_ON_FRAME))) {
                GST_DEBUG_OBJECT(mybase_, "Got last ROI flag in buffer %p", buf);
                break;
            }
        }

        return GST_FLOW_OK;
    }

    void mergeMetadata() {
        g_assert(current_buf_);

        current_buf_ = gst_buffer_make_writable(current_buf_);

        GST_DEBUG_OBJECT(mybase_, "Merging %lu buffers w/meta to buffer %p ts=%" GST_TIME_FORMAT,
                         current_meta_bufs_.size(), current_buf_, GST_TIME_ARGS(GST_BUFFER_PTS(current_buf_)));
        while (!current_meta_bufs_.empty()) {
            auto buf = current_meta_bufs_.front();
            current_meta_bufs_.pop_front();

            mergeMetaFromBuffer(buf);

            gst_buffer_unref(buf);
        }
    }

    bool mergeMetaFromBuffer(GstBuffer *buf_with_meta) {
        g_assert(current_buf_);
        g_assert(buf_with_meta);
        g_assert(gst_buffer_is_writable(current_buf_) && "Current buffer from video pad is not writable");

        gpointer state = nullptr;
        GstMeta *meta = nullptr;

        // Get reference roi id to attach meta to
        auto reference_roi_id = -1;
        // FIXME: remove code below after full transition to dlstreamer::SourceIdentifierMetadata
        if (roi_ref_meta_api_ != 0) {
            auto roi_ref_meta =
                reinterpret_cast<GvaRoiRefMeta *>(gst_buffer_get_meta(buf_with_meta, roi_ref_meta_api_));
            if (roi_ref_meta)
                reference_roi_id = roi_ref_meta->reference_roi_id;
        }

        dlstreamer::GSTBuffer dls_buf_with_meta(buf_with_meta, &video_info_);
        for (auto &dls_meta : dls_buf_with_meta.metadata()) {
            auto source_id_meta = dlstreamer::SourceIdentifierMetadata::try_cast(dls_meta);
            if (source_id_meta) {
                reference_roi_id = source_id_meta->roi_id();
                break;
            }
        }

        GstMetaTransformCopy meta_copy_param = {.region = false, .offset = 0, .size = static_cast<gsize>(-1)};

        while ((meta = gst_buffer_iterate_meta(buf_with_meta, &state))) {
            if (meta->info->api == roi_ref_meta_api_)
                // We only need roi_id from it
                continue;

            // If we got tensor meta and roi reference id then we try to attach it to ROI in video buffer
            if (reference_roi_id > -1 && meta->info->api == gst_gva_tensor_meta_api_get_type()) {
                auto tensor_meta = reinterpret_cast<GstGVATensorMeta *>(meta);
                std::string name = g_quark_to_string(tensor_meta->data->name);
                if (name == dlstreamer::SourceIdentifierMetadata::name) // We only need roi_id from it
                    continue;
                auto reference_roi_meta =
                    gst_buffer_get_video_region_of_interest_meta_id(current_buf_, reference_roi_id);
                if (reference_roi_meta) {
                    gst_video_region_of_interest_meta_add_param(reference_roi_meta,
                                                                gst_structure_copy(tensor_meta->data));
                    continue;
                }

                GST_WARNING_OBJECT(mybase_, "Cannot find ROI with id %d. Tensor will be attached as full frame",
                                   reference_roi_id);
            }

            // Parse metadata created by postproc_ elements via GSTBuffer::add_metadata("detection")
            if (reference_roi_id == -1 && meta->info->api == gst_gva_tensor_meta_api_get_type()) {
                auto tensor_meta = reinterpret_cast<GstGVATensorMeta *>(meta);
                std::string name = g_quark_to_string(tensor_meta->data->name);
                if (name == "detection") {
                    GstStructure *structure = gst_structure_copy(tensor_meta->data); // copy "detection" structure
                    auto label = gst_structure_get_string(structure, "label");
                    GstVideoRegionOfInterestMeta *roi_meta =
                        gst_buffer_add_video_region_of_interest_meta(current_buf_, label, 0, 0, 0, 0);
                    scaleRoi_(roi_meta, tensor_meta->data, nullptr);
                    gst_video_region_of_interest_meta_add_param(roi_meta, structure);
                    continue;
                }
            }

            if (meta->info->api == GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE) {
                auto roi_meta = reinterpret_cast<GstVideoRegionOfInterestMeta *>(meta);
                // Scale ROI by detection param. It will be copied to video buffer further
                auto detection = gst_video_region_of_interest_meta_get_param(roi_meta, "detection");
                if (detection)
                    scaleRoi_(roi_meta, detection,
                              gst_buffer_get_video_region_of_interest_meta_id(current_buf_, reference_roi_id));
                else
                    GST_WARNING_OBJECT(mybase_, "Got ROI meta without detection param. Will be copied as is");
            }

            // Try to copy the whole meta from sink buffer to out buffer
            if (!meta->info->transform_func(current_buf_, meta, buf_with_meta, _gst_meta_transform_copy,
                                            &meta_copy_param)) {
                GST_ERROR_OBJECT(mybase_, "Failed to copy metadata to out buffer");
                return false;
            }
        }

        return true;
    }

    void scaleRoi_(GstVideoRegionOfInterestMeta *roi_meta, GstStructure *detection,
                   GstVideoRegionOfInterestMeta *parent_roi) {
        g_assert(detection);
        g_assert(roi_meta);

        const GstVideoInfo *video_info = &video_info_;

        double x_min, x_max, y_min, y_max;
        gst_structure_get_double(detection, "x_min", &x_min);
        gst_structure_get_double(detection, "x_max", &x_max);
        gst_structure_get_double(detection, "y_min", &y_min);
        gst_structure_get_double(detection, "y_max", &y_max);

        // clip to [0, 1] range
        if (!((x_min >= 0) && (y_min >= 0) && (x_max <= +1) && (y_max <= +1))) {
            GST_DEBUG_OBJECT(
                mybase_, "ROI coordinates x=[%.5f, %.5f], y=[%.5f, %.5f] are out of range [0,1] and will be clipped",
                x_min, x_max, y_min, y_max);
            x_min = std::clamp(x_min, 0., 1.);
            y_min = std::clamp(y_min, 0., 1.);
            x_max = std::clamp(x_max, 0., 1.);
            y_max = std::clamp(y_max, 0., 1.);
        }

        /* calculate scaled coords */
        auto parent_width = parent_roi ? parent_roi->w : video_info->width;
        auto parent_height = parent_roi ? parent_roi->h : video_info->height;
        auto x_offset = parent_roi ? parent_roi->x : 0;
        auto y_offset = parent_roi ? parent_roi->y : 0;
        roi_meta->x = static_cast<uint32_t>(x_min * parent_width + 0.5) + x_offset;
        roi_meta->y = static_cast<uint32_t>(y_min * parent_height + 0.5) + y_offset;
        roi_meta->w = static_cast<uint32_t>((x_max - x_min) * parent_width + 0.5);
        roi_meta->h = static_cast<uint32_t>((y_max - y_min) * parent_height + 0.5);

        if (parent_roi) {
            // In case of parent roi we need to change detection values relative to full frame
            x_min = std::clamp(roi_meta->x / static_cast<double>(video_info->width), 0., 1.);
            y_min = std::clamp(roi_meta->y / static_cast<double>(video_info->height), 0., 1.);
            x_max = std::clamp((roi_meta->x + roi_meta->w) / static_cast<double>(video_info->width), 0., 1.);
            y_max = std::clamp((roi_meta->y + roi_meta->h) / static_cast<double>(video_info->height), 0., 1.);
        }

        gst_structure_set(detection, "x_min", G_TYPE_DOUBLE, x_min, "x_max", G_TYPE_DOUBLE, x_max, "y_min",
                          G_TYPE_DOUBLE, y_min, "y_max", G_TYPE_DOUBLE, y_max, NULL);
    }

  private:
    GstAggregator *mybase_;
    GstCaps *current_caps_ = nullptr;
    GstVideoInfo video_info_ = {};

    GstBuffer *current_buf_ = nullptr;
    GstClockTime current_running_time_ = GST_CLOCK_TIME_NONE;
    GstClockTime current_running_time_end_ = GST_CLOCK_TIME_NONE;

    std::list<GstBuffer *> current_meta_bufs_;

    uint32_t tensor_pad_num_ = 0;
    GType roi_ref_meta_api_ = 0;
};

// Define type after private data
G_DEFINE_TYPE_WITH_PRIVATE(MetaAggregate, meta_aggregate, GST_TYPE_AGGREGATOR);

gboolean MetaAggregatePrivate::sinkEvent(GstAggregatorPad *pad, GstEvent *event) {
    gboolean ret = TRUE;

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_CAPS: {
        if (pad != GST_AGGREGATOR_PAD(firstSink()))
            break;

        // Set caps from first pad as our source caps
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);

        current_caps_ = gst_caps_copy(caps);
        gst_video_info_from_caps(&video_info_, caps);
        gst_aggregator_set_src_caps(mybase_, caps);
        GST_INFO_OBJECT(mybase_, "src caps set: %" GST_PTR_FORMAT, caps);
        break;
    }

    /* It is needed, otherwise timestamps might be incorrect */
    // gst_aggregator_update_segment is available starting from 1.18
#if 1
    case GST_EVENT_SEGMENT: {
        if (strcmp(GST_OBJECT_NAME(pad), "sink") == 0) {
            const GstSegment *segment;

            gst_event_parse_segment(event, &segment);
            gst_aggregator_update_segment(mybase_, segment);
        }
        break;
    }
#endif

#if 0
        case GST_EVENT_TAG: {
            GstTagList *list;
            GstTagSetter *setter = GST_TAG_SETTER(mux);
            const GstTagMergeMode mode = gst_tag_setter_get_tag_merge_mode(setter);

            gst_event_parse_tag(event, &list);
            gst_tag_setter_merge_tags(setter, list, mode);
            gst_flv_mux_store_codec_tags(mux, flvpad, list);
            mux->new_tags = TRUE;
            ret = TRUE;
            break;
        }
#endif
    default:
        break;
    }

    if (!ret)
        return FALSE;

    return GST_AGGREGATOR_CLASS(meta_aggregate_parent_class)->sink_event(mybase_, pad, event);
}

gboolean MetaAggregatePrivate::srcQuery(GstQuery *query) {
    gboolean ret;

    switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_POSITION:
    case GST_QUERY_DURATION:
    case GST_QUERY_URI:
    case GST_QUERY_CAPS:
    case GST_QUERY_ALLOCATION: {
        GstPad *main_sinkpad = GST_PAD(firstSink());
        ret = gst_pad_peer_query(main_sinkpad, query);
        break;
    }

    case GST_QUERY_ACCEPT_CAPS: {
        GstCaps *caps;
        GstCaps *templ = gst_static_pad_template_get_caps(&src_templ);

        gst_query_parse_accept_caps(query, &caps);
        gst_query_set_accept_caps_result(query, gst_caps_is_subset(caps, templ));
        gst_caps_unref(templ);
        ret = TRUE;
        break;
    }

    default:
        ret = GST_AGGREGATOR_CLASS(meta_aggregate_parent_class)->src_query(mybase_, query);
        break;
    }

    return ret;
}

namespace {

void meta_aggregate_finalize(GObject *object) {
    MetaAggregate *mux = GST_META_AGGREGATE(object);
    GST_INFO_OBJECT(mux, "metaaggregate finalize!");
    g_assert(mux->impl);

    if (mux->impl) {
        mux->impl->~MetaAggregatePrivate();
        mux->impl = nullptr;
    }

    G_OBJECT_CLASS(meta_aggregate_parent_class)->finalize(object);
}

} // namespace

static void meta_aggregate_class_init(MetaAggregateClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);
    GstAggregatorClass *gstaggregator_class = GST_AGGREGATOR_CLASS(klass);

    GST_DEBUG_CATEGORY_INIT(meta_aggregate_debug, "metaaggregate", 0, "Tensor muxer");

    gobject_class->finalize = meta_aggregate_finalize;

    // gstaggregator_class->start = GST_DEBUG_FUNCPTR(metaaggregate_start);
    // TODO: Figure if it possible to use GST_DEBUG_FUNCPTR
    gstaggregator_class->aggregate = [](GstAggregator *aggregator, gboolean timeout) {
        return GST_META_AGGREGATE(aggregator)->impl->aggregate(timeout != false);
    };
    gstaggregator_class->sink_event = [](GstAggregator *aggregator, GstAggregatorPad *pad, GstEvent *event) {
        return GST_META_AGGREGATE(aggregator)->impl->sinkEvent(pad, event);
    };
    gstaggregator_class->src_query = [](GstAggregator *aggregator, GstQuery *query) {
        return GST_META_AGGREGATE(aggregator)->impl->srcQuery(query);
    };
    gstaggregator_class->update_src_caps = [](GstAggregator *aggregator, GstCaps *caps, GstCaps **ret) {
        return GST_META_AGGREGATE(aggregator)->impl->updateSrcCaps(caps, ret);
    };
    gstaggregator_class->create_new_pad = [](GstAggregator *aggregator, GstPadTemplate *templ, const gchar *req_name,
                                             const GstCaps *caps) {
        return GST_META_AGGREGATE(aggregator)->impl->createNewPad(templ, req_name, caps);
    };

    gstaggregator_class->negotiate = NULL;

    // gstaggregator_class->flush = GST_DEBUG_FUNCPTR(metaaggregate_flush);
    // gstaggregator_class->get_next_time = GST_DEBUG_FUNCPTR(metaaggregate_get_next_time);

    gst_element_class_add_static_pad_template_with_gtype(gstelement_class, &tensor_templ, GST_TYPE_META_AGGREGATE_PAD);
    gst_element_class_add_static_pad_template_with_gtype(gstelement_class, &sink_templ, GST_TYPE_META_AGGREGATE_PAD);
    gst_element_class_add_static_pad_template_with_gtype(gstelement_class, &src_templ, GST_TYPE_AGGREGATOR_PAD);
    gst_element_class_set_static_metadata(gstelement_class, "[Preview] Tensor AV Muxer", "Codec/Muxer",
                                          "Muxes video streams with tensor's ROI into into single stream",
                                          "Intel Corporation");

#if 0
    // Since 1.18
    gst_type_mark_as_plugin_api(GST_TYPE_META_AGGREGATE_PAD, 0);
#else
    g_type_class_ref(GST_TYPE_META_AGGREGATE_PAD);
#endif
}

static void meta_aggregate_init(MetaAggregate *self) {
    GST_INFO_OBJECT(self, "metaaggregate init!");

    auto templ = gst_static_pad_template_get(&sink_templ);
    auto pad = g_object_new(GST_TYPE_META_AGGREGATE_PAD, "name", "sink", "direction", GST_PAD_SINK, "template", templ,
                            nullptr);
    gst_object_unref(templ);
    gst_element_add_pad(GST_ELEMENT_CAST(self), GST_PAD_CAST(pad));

    // Intialization of private data
    auto *priv_memory = meta_aggregate_get_instance_private(self);
    self->impl = new (priv_memory) MetaAggregatePrivate(&self->parent);
}
