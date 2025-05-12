/*******************************************************************************
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "meta_aggregate.h"
#include "dlstreamer/gst/frame.h"
#include "dlstreamer/gst/mappers/gst_to_cpu.h"
#include "dlstreamer/gst/metadata/gva_tensor_meta.h"
#include "dlstreamer/image_metadata.h"
#include "roi_split.h"
#include <list>

using namespace dlstreamer;

GST_DEBUG_CATEGORY_STATIC(meta_aggregate_debug);
#define GST_CAT_DEFAULT meta_aggregate_debug

enum { PROP_0, PROP_ATTACH_TENSOR_DATA };

#define DEFAULT_ATTACH_TENSOR_DATA TRUE
#define EVENT_TAG 0

// ---
// MetaAggregatePadPriv and MetaAggregatePad

class MetaAggregatePadPrivate {
  public:
    MetaAggregatePadPrivate(GstAggregatorPad *parent) : _mybase(parent) {
        gst_video_info_init(&_video_info);
    }

    void update_current_caps(const GstCaps *caps) {
        _frame_info = gst_caps_to_frame_info(caps);
        if (_frame_info.media_type == MediaType::Image)
            _video_info_valid = gst_video_info_from_caps(&_video_info, caps);
        else
            _video_info_valid = false;
    }

    MediaType media_type() const {
        return _frame_info.media_type;
    }

    // Returns true if media type on this pad is tensors
    bool is_tensors_pad() const {
        return media_type() == MediaType::Tensors;
    }

    // Returns GstVideoInfo if pad's media type is video/x-raw, otherwise returns nullptr
    const GstVideoInfo *video_info() const {
        return _video_info_valid ? &_video_info : nullptr;
    }

    const FrameInfo &frame_info() const {
        return _frame_info;
    }

  private:
    GstAggregatorPad *_mybase;
    bool _video_info_valid = false;
    GstVideoInfo _video_info;
    FrameInfo _frame_info;
};

// Define type after private data
G_DEFINE_TYPE_WITH_PRIVATE(MetaAggregatePad, meta_aggregate_pad, GST_TYPE_AGGREGATOR_PAD);

static void meta_aggregate_pad_init(MetaAggregatePad *self) {
    // Intialization of private data
    auto *priv_memory = meta_aggregate_pad_get_instance_private(self);
    // This won't be converted to shared ptr because of memory placement
    self->impl = new (priv_memory) MetaAggregatePadPrivate(&self->parent);
}

void meta_aggregate_pad_finalize(GObject *object) {
    MetaAggregatePad *pad = GST_META_AGGREGATE_PAD(object);
    g_assert(pad->impl);

    if (pad->impl) {
        pad->impl->~MetaAggregatePadPrivate();
        pad->impl = nullptr;
    }

    G_OBJECT_CLASS(meta_aggregate_pad_parent_class)->finalize(object);
}

static void meta_aggregate_pad_class_init(MetaAggregatePadClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = meta_aggregate_pad_finalize;
}

// ----

namespace {
GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

GstStaticPadTemplate sink_templ = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

// Templates for request pads, metadata from which should be merged.
GstStaticPadTemplate request_templs[] = {
    // Video
    GST_STATIC_PAD_TEMPLATE("meta_%u", GST_PAD_SINK, GST_PAD_REQUEST, GST_STATIC_CAPS("video/x-raw(ANY)")),
    // Tensor
    GST_STATIC_PAD_TEMPLATE("tensor_%u", GST_PAD_SINK, GST_PAD_REQUEST,
                            GST_STATIC_CAPS(DLS_TENSOR_MEDIA_NAME "(ANY)"))};

} // namespace

class MetaAggregatePrivate {
  public:
    MetaAggregatePrivate(GstAggregator *parent) : mybase_(parent), gst_to_cpu_(nullptr, nullptr) {
    }

    ~MetaAggregatePrivate() {
        // TODO
        if (current_caps_)
            gst_caps_unref(current_caps_);
    }

    MetaAggregatePrivate(const MetaAggregatePrivate &) = delete;
    MetaAggregatePrivate &operator=(const MetaAggregatePrivate &) = delete;

    void set_property(guint prop_id, const GValue *value) {
        switch (prop_id) {
        case PROP_ATTACH_TENSOR_DATA:
            attach_tensor_data_ = g_value_get_boolean(value);
            return;
        }
        throw std::runtime_error("Invalid property " + std::to_string(prop_id));
    }

    void get_property(guint prop_id, GValue *value) {
        switch (prop_id) {
        case PROP_ATTACH_TENSOR_DATA:
            g_value_set_boolean(value, attach_tensor_data_);
            return;
        }
        throw std::runtime_error("Invalid property " + std::to_string(prop_id));
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

        // Look for counter for specific pad template
        uint32_t *counter = nullptr;
        for (size_t i = 0; i < std::size(request_templs); i++) {
            if (g_str_equal(templ->name_template, request_templs[i].name_template))
                counter = &request_pad_counters_[i];
        }

        if (!counter) {
            GST_WARNING_OBJECT(mybase_, "Unrecognized pad template name: %s", templ->name_template);
            return nullptr;
        }

        GST_OBJECT_LOCK(mybase_);
        gchar *name = g_strdup_printf(templ->name_template, *counter++);
        auto *res_pad = reinterpret_cast<MetaAggregatePad *>(g_object_new(
            GST_TYPE_META_AGGREGATE_PAD, "name", name, "direction", GST_PAD_SINK, "template", templ, nullptr));
        g_free(name);
        GST_DEBUG_OBJECT(mybase_, "REQUEST pad created: %" GST_PTR_FORMAT, res_pad);
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
            GST_ERROR_OBJECT(mybase_, "GST_AGGREGATOR_FLOW_NEED_DATA");
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
        GST_DEBUG_OBJECT(mybase_,
                         "Selected current buffer %p, %" GST_TIME_FORMAT " running time: %" GST_TIME_FORMAT
                         " -> %" GST_TIME_FORMAT,
                         current_buf_, GST_TIME_ARGS(GST_BUFFER_PTS(current_buf_)),
                         GST_TIME_ARGS(current_running_time_), GST_TIME_ARGS(current_running_time_end_));

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
            GstBuffer *meta_buf = gst_aggregator_pad_peek_buffer(&pad->parent);
            if (!meta_buf) {
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

            GstClockTime buf_time = GST_BUFFER_PTS(meta_buf);
            if (!GST_CLOCK_TIME_IS_VALID(buf_time)) {
                GST_ERROR_OBJECT(mybase_, "Got buffer without PTS on pad %" GST_PTR_FORMAT, pad);

                gst_buffer_unref(meta_buf);
                return GST_FLOW_ERROR;
            }

            buf_time = gst_segment_to_running_time(&pad->parent.segment, GST_FORMAT_TIME, buf_time);
            if (!GST_CLOCK_TIME_IS_VALID(buf_time)) {
                GST_DEBUG_OBJECT(mybase_, "Buffer %" GST_PTR_FORMAT " outside segment -> dropping", meta_buf);
                gst_aggregator_pad_drop_buffer(&pad->parent);
                gst_buffer_unref(meta_buf);

                continue;
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
                gst_buffer_unref(meta_buf);
                break;
            }

            if (gst_buffer_has_flags(meta_buf, GST_BUFFER_FLAG_GAP)) {
                GST_DEBUG_OBJECT(mybase_, "Buffer %" GST_PTR_FORMAT " with GAP -> dropping", meta_buf);
                gst_aggregator_pad_drop_buffer(&pad->parent);
                gst_buffer_unref(meta_buf);

                // FIXME:
                // Ideally here should be continue. However, when identity eos-after=X is used this causes pipeline hang
                // So, it requires additional investigation
                break;
            }

            GST_DEBUG_OBJECT(mybase_, "Collecting metadata buffer %p %" GST_TIME_FORMAT " for current buffer %p",
                             meta_buf, GST_TIME_ARGS(buf_time), current_buf_);

            gst_aggregator_pad_drop_buffer(&pad->parent);

            GstFramePtr frame;
            if (pad->impl->video_info()) {
                assert(pad->impl->media_type() == MediaType::Image);
                // true -> take ownership of buffer
                frame = std::make_shared<GSTFrame>(meta_buf, pad->impl->video_info(), nullptr, true);
            } else {
                assert(pad->impl->media_type() == MediaType::Tensors);
                // true -> take ownership of buffer
                frame = std::make_shared<GSTFrame>(meta_buf, pad->impl->frame_info(), true);
            }

            if (frame)
                current_meta_bufs_.push_back(std::move(frame));
            else
                GST_ERROR_OBJECT(mybase_, "Failed to create internal frame object from buffer %p on pad %p", meta_buf,
                                 pad);

            // Early exit if we found our custom flag
            if (gst_buffer_has_flags(meta_buf, static_cast<GstBufferFlags>(DLS_BUFFER_FLAG_LAST_ROI_ON_FRAME))) {
                GST_DEBUG_OBJECT(mybase_, "Got last ROI flag in buffer %p", meta_buf);
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
            auto meta_frame = current_meta_bufs_.front();
            current_meta_bufs_.pop_front();

            if (!mergeMetaFromFrame(std::move(meta_frame))) {
                GST_WARNING_OBJECT(mybase_, "Failed to merge metadata");
            }
        }
    }

    bool mergeMetaFromFrame(GstFramePtr dls_buf_with_meta) {
        g_assert(current_buf_);
        g_assert(gst_buffer_is_writable(current_buf_) && "Current buffer from video pad is not writable");

        switch (dls_buf_with_meta->media_type()) {
        case MediaType::Image:
            return mergeMetaFromVideoFrame(std::move(dls_buf_with_meta));

        case MediaType::Tensors:
            return mergeMetaFromTensorFrame(std::move(dls_buf_with_meta));

        default:
            break;
        }

        return false;
    }

    bool mergeMetaFromVideoFrame(GstFramePtr frame) {
        gpointer state = nullptr;
        GstMeta *meta;
        while ((meta = gst_buffer_iterate_meta_filtered(frame->gst_buffer(), &state,
                                                        GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE))) {
            auto *roi_meta_to_merge = reinterpret_cast<GstVideoRegionOfInterestMeta *>(meta);

            // Get detection metadata
            GstStructure *structure =
                gst_video_region_of_interest_meta_get_param(roi_meta_to_merge, DetectionMetadata::name);
            if (!structure) {
                GST_WARNING_OBJECT(mybase_,
                                   "Skipping ROI %p from buffer %p because detection metadata for ROI is missing",
                                   roi_meta_to_merge, frame->gst_buffer());
                continue;
            }

            // FIXME: avoid copy by taking ownership?
            structure = gst_structure_copy(structure);
            const gchar *label = gst_structure_get_string(structure, "label");
            if (!label)
                label = g_quark_to_string(roi_meta_to_merge->roi_type);

            GstVideoRegionOfInterestMeta *roi_meta =
                gst_buffer_add_video_region_of_interest_meta(current_buf_, label, 0, 0, 0, 0);

            // FIXME: scale ?

            gst_video_region_of_interest_meta_add_param(roi_meta, structure);
            roi_meta->id = gst_util_seqnum_next();
        }

        return true;
    }

    bool mergeMetaFromTensorFrame(GstFramePtr meta_frame) {
        g_assert(meta_frame && "Meta frame must be valid");
        // Find SourceIdentifierMetadata and corresponding ROI meta if inference-region=per-roi
        GstVideoRegionOfInterestMeta *parent_roi_meta = nullptr;
        auto source_id_meta = find_metadata<SourceIdentifierMetadata>(*meta_frame);
        if (source_id_meta) {
            int roi_id = source_id_meta->roi_id();
            if (roi_id != GST_SEQNUM_INVALID) { // non-zero
                parent_roi_meta = gst_buffer_get_video_region_of_interest_meta_id(current_buf_, roi_id);
                if (!parent_roi_meta)
                    GST_WARNING_OBJECT(mybase_, "Can't find ROI by id: %d", roi_id);
            }
        }

        std::vector<std::string> output_layers;
        auto model_info_meta = find_metadata<ModelInfoMetadata>(*meta_frame);
        if (model_info_meta)
            output_layers = model_info_meta->output_layers();

        GstGVATensorMeta *custom_meta;
        gpointer state = nullptr;
        while ((custom_meta = GST_GVA_TENSOR_META_ITERATE(meta_frame->gst_buffer(), &state))) {
            std::string name = custom_meta->data->name ? g_quark_to_string(custom_meta->data->name) : "";
            // Skip utility metadata-s
            if (name == SourceIdentifierMetadata::name || name == ModelInfoMetadata::name ||
                name == AffineTransformInfoMetadata::name)
                continue;
#if 0
            // Take ownership of GstStructure
            GstStructure *out_tensor_data = custom_meta->data;
            custom_meta->data = nullptr;
#else
            GstStructure *out_tensor_data = gst_structure_copy(custom_meta->data);
#endif

            // Copy tensor data to GstStructure if requested by property and tensor data not attached yet
            if (attach_tensor_data_ && !gst_structure_has_field(out_tensor_data, "data_buffer")) {
                FramePtr cpu_buffer = gst_to_cpu_.map(meta_frame, AccessMode::Read);
                for (size_t i = 0; i < cpu_buffer->num_tensors(); i++) {
                    InferenceResultMetadata inference_meta(std::make_shared<GSTDictionary>(out_tensor_data));
                    std::string layer_name = (i < output_layers.size()) ? output_layers[i] : "";
                    inference_meta.init_tensor_data(*cpu_buffer->tensor(i), layer_name);
                }
            }
            // Attach to output buffer
            if (name == DetectionMetadata::name) { // attach as GstVideoRegionOfInterestMeta
                auto label = gst_structure_get_string(out_tensor_data, DetectionMetadata::key::label);
                GstVideoRegionOfInterestMeta *roi_meta =
                    gst_buffer_add_video_region_of_interest_meta(current_buf_, label, 0, 0, 0, 0);

                auto affine_transform = find_metadata<AffineTransformInfoMetadata>(*meta_frame);

                scaleRoi_(roi_meta, out_tensor_data, nullptr, affine_transform.get());

                gst_video_region_of_interest_meta_add_param(roi_meta, out_tensor_data);
                roi_meta->id = gst_util_seqnum_next();
                if (parent_roi_meta)
                    roi_meta->parent_id = parent_roi_meta->id;
            } else { // attach as GstGVATensorMeta (full-frame) or param in GstVideoRegionOfInterestMeta (per-roi)
                if (parent_roi_meta) {
                    gst_video_region_of_interest_meta_add_param(parent_roi_meta, out_tensor_data);
                } else {
                    GstGVATensorMeta *tensor = GST_GVA_TENSOR_META_ADD(current_buf_);
                    if (tensor->data)
                        gst_structure_free(tensor->data);
                    tensor->data = out_tensor_data;
                }
            }
        }

        return true;
    }

    std::pair<double, double> applyMatrix(double x, double y, double *m) { // 3x2 matrix
        double xx = x * m[0] + y * m[1] + m[2];
        double yy = x * m[3] + y * m[4] + m[5];
        return {xx, yy};
    }

    void scaleRoi_(GstVideoRegionOfInterestMeta *roi_meta, GstStructure *detection,
                   GstVideoRegionOfInterestMeta *parent_roi, AffineTransformInfoMetadata *affine_transform = nullptr) {
        g_assert(detection);
        g_assert(roi_meta);

        const GstVideoInfo *video_info = &video_info_;

        double x_min, x_max, y_min, y_max;
        gst_structure_get_double(detection, DetectionMetadata::key::x_min, &x_min);
        gst_structure_get_double(detection, DetectionMetadata::key::x_max, &x_max);
        gst_structure_get_double(detection, DetectionMetadata::key::y_min, &y_min);
        gst_structure_get_double(detection, DetectionMetadata::key::y_max, &y_max);

        // In case affine transform was applied (resize, crop, rotate, etc), multiply coordinates by transform matrix
        if (affine_transform) {
            auto matrix = affine_transform->matrix();
            if (matrix.size() < 6)
                throw std::runtime_error("Expect AffineTransformInfoMetadata with matrix size equal to 6");
            std::tie(x_min, y_min) = applyMatrix(x_min, y_min, matrix.data());
            std::tie(x_max, y_max) = applyMatrix(x_max, y_max, matrix.data());
        }

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

        gst_structure_set(detection, DetectionMetadata::key::x_min, G_TYPE_DOUBLE, x_min, DetectionMetadata::key::x_max,
                          G_TYPE_DOUBLE, x_max, DetectionMetadata::key::y_min, G_TYPE_DOUBLE, y_min,
                          DetectionMetadata::key::y_max, G_TYPE_DOUBLE, y_max, NULL);
    }

  private:
    GstAggregator *mybase_;
    gboolean attach_tensor_data_ = DEFAULT_ATTACH_TENSOR_DATA;
    GstCaps *current_caps_ = nullptr;
    GstVideoInfo video_info_ = {};

    GstBuffer *current_buf_ = nullptr;
    GstClockTime current_running_time_ = GST_CLOCK_TIME_NONE;
    GstClockTime current_running_time_end_ = GST_CLOCK_TIME_NONE;

    std::list<GstFramePtr> current_meta_bufs_;

    uint32_t request_pad_counters_[std::size(request_templs)] = {};
    MemoryMapperGSTToCPU gst_to_cpu_;
};

// Define type after private data
G_DEFINE_TYPE_WITH_PRIVATE(MetaAggregate, meta_aggregate, GST_TYPE_AGGREGATOR);

gboolean MetaAggregatePrivate::sinkEvent(GstAggregatorPad *pad, GstEvent *event) {

#if EVENT_TAG
    gboolean ret = TRUE;
#endif

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_CAPS: {
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);
        if (pad == GST_AGGREGATOR_PAD(firstSink())) {
            // Set caps from first pad as our source caps
            current_caps_ = gst_caps_copy(caps);
            gst_video_info_from_caps(&video_info_, caps);
            gst_aggregator_set_src_caps(mybase_, caps);
            GST_INFO_OBJECT(mybase_, "src caps set: %" GST_PTR_FORMAT, caps);
        } else {
            auto mypad = GST_META_AGGREGATE_PAD(pad);
            mypad->impl->update_current_caps(caps);
        }
        break;
    }

// Check if current GStreamer version is 1.18 or greater
#if GST_CHECK_VERSION(1, 18, 0)
    // If so, update the event segment to ensure timestamps are correct
    // gst_aggregator_update_segment is available starting from 1.18
    case GST_EVENT_SEGMENT: {
        if (strcmp(GST_OBJECT_NAME(pad), "sink") == 0) {
            const GstSegment *segment;

            gst_event_parse_segment(event, &segment);
            gst_aggregator_update_segment(mybase_, segment);
        }
        break;
    }
#endif

#if EVENT_TAG
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

#if EVENT_TAG
    if (!ret)
        return FALSE;
#endif

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

    GST_DEBUG_CATEGORY_INIT(meta_aggregate_debug, "meta_aggregate", 0, "Tensor muxer");

    gobject_class->finalize = meta_aggregate_finalize;

    // gstaggregator_class->start = GST_DEBUG_FUNCPTR(metaaggregate_start);
    // TODO: Figure if it possible to use GST_DEBUG_FUNCPTR
    gobject_class->set_property = [](GObject *object, guint prop_id, const GValue *value, GParamSpec *) {
        return GST_META_AGGREGATE(object)->impl->set_property(prop_id, value);
    };
    gobject_class->get_property = [](GObject *object, guint property_id, GValue *value, GParamSpec *) {
        GST_META_AGGREGATE(object)->impl->get_property(property_id, value);
    };
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

    // gstaggregator_class->flush = GST_DEBUG_FUNCPTR(metaaggregate_flush);
    // gstaggregator_class->get_next_time = GST_DEBUG_FUNCPTR(metaaggregate_get_next_time);

    for (auto &templ : request_templs) {
        gst_element_class_add_static_pad_template_with_gtype(gstelement_class, &templ, GST_TYPE_META_AGGREGATE_PAD);
    }
    gst_element_class_add_static_pad_template_with_gtype(gstelement_class, &sink_templ, GST_TYPE_META_AGGREGATE_PAD);
    gst_element_class_add_static_pad_template_with_gtype(gstelement_class, &src_templ, GST_TYPE_AGGREGATOR_PAD);
    gst_element_class_set_static_metadata(gstelement_class, "[Preview] Tensor AV Muxer", "Codec/Muxer",
                                          "Muxes video streams with tensor's ROI into into single stream",
                                          "Intel Corporation");

    g_object_class_install_property(
        gobject_class, PROP_ATTACH_TENSOR_DATA,
        g_param_spec_boolean("attach-tensor-data", "attach-tensor-data",
                             "If true, additionally copies tensor data into metadata", DEFAULT_ATTACH_TENSOR_DATA,
                             static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

#if GST_CHECK_VERSION(1, 18, 0)
    // Since 1.18
    gst_type_mark_as_plugin_api(GST_TYPE_META_AGGREGATE_PAD, static_cast<GstPluginAPIFlags>(0));
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
    // This won't be converted to shared ptr because of memory placement
    self->impl = new (priv_memory) MetaAggregatePrivate(&self->parent);
}
