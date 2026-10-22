/*******************************************************************************
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "video_dump_writer.hpp"

#include <gst/app/gstappsrc.h>
#include <gst/gst.h>

GST_DEBUG_CATEGORY_EXTERN(gst_gvaeventrecorder_debug);
#define GST_CAT_DEFAULT gst_gvaeventrecorder_debug

namespace gvaeventrecorder {

namespace {

// AUTO prefers the hardware VA-API encoder and falls back to software when unavailable.
std::string pick_encoder(EncoderMode mode) {
    auto factory_available = [](const char *name) {
        GstElementFactory *f = gst_element_factory_find(name);
        if (f)
            gst_object_unref(f);
        return f != nullptr;
    };

    // x264enc lives in gst-plugins-ugly and is often not installed; openh264enc is the fallback.
    const std::string software =
        factory_available("x264enc") ? "x264enc tune=zerolatency speed-preset=ultrafast" : "openh264enc";

    switch (mode) {
    case EncoderMode::VaApi:
        return "vah264enc";
    case EncoderMode::Auto:
        return factory_available("vah264enc") ? "vah264enc" : software;
    case EncoderMode::Software:
    default:
        return software;
    }
}

void unref_all(const std::vector<GstBuffer *> &frames) {
    for (GstBuffer *b : frames)
        gst_buffer_unref(b);
}

} // namespace

VideoDumpWriter::VideoDumpWriter() = default;

VideoDumpWriter::~VideoDumpWriter() {
    stop();
}

void VideoDumpWriter::start() {
    if (running_)
        return;
    stop_requested_ = false;
    running_ = true;
    thread_ = std::thread(&VideoDumpWriter::worker_main, this);
}

void VideoDumpWriter::stop() {
    if (!running_)
        return;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_requested_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable())
        thread_.join();
    running_ = false;
}

void VideoDumpWriter::enqueue(DumpJob job) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(job));
    }
    cv_.notify_one();
}

void VideoDumpWriter::worker_main() {
    for (;;) {
        DumpJob job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stop_requested_ || !queue_.empty(); });
            if (queue_.empty()) {
                if (stop_requested_)
                    return;
                continue;
            }
            job = std::move(queue_.front());
            queue_.pop();
        }
        std::string error;
        if (!write_job(job, error))
            GST_WARNING("Failed to write event recording '%s': %s", job.output_path.c_str(), error.c_str());
    }
}

bool VideoDumpWriter::write_job(const DumpJob &job, std::string &error_out) {
    if (job.frames.empty()) {
        error_out = "no frames to write";
        return false;
    }

    const std::string desc = "appsrc name=src format=time is-live=false block=true ! videoconvert ! " +
                             pick_encoder(job.encoder_mode) + " ! h264parse ! mp4mux ! filesink name=sink";

    GError *gerr = nullptr;
    GstElement *pipeline = gst_parse_launch(desc.c_str(), &gerr);
    if (!pipeline) {
        error_out = gerr ? gerr->message : "gst_parse_launch failed";
        if (gerr)
            g_error_free(gerr);
        unref_all(job.frames);
        return false;
    }

    GstElement *appsrc = gst_bin_get_by_name(GST_BIN(pipeline), "src");
    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    g_object_set(sink, "location", job.output_path.c_str(), NULL);

    GstCaps *caps = gst_video_info_to_caps(const_cast<GstVideoInfo *>(&job.video_info));
    g_object_set(appsrc, "caps", caps, "format", GST_FORMAT_TIME, NULL);
    gst_caps_unref(caps);

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        error_out = "failed to start internal recording pipeline";
        unref_all(job.frames);
        gst_object_unref(appsrc);
        gst_object_unref(sink);
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return false;
    }

    const guint64 duration_per_frame =
        (job.video_info.fps_n > 0) ? gst_util_uint64_scale(GST_SECOND, job.video_info.fps_d, job.video_info.fps_n)
                                   : GST_SECOND / 30;

    guint64 pts = 0;
    bool push_failed = false;
    for (GstBuffer *buf : job.frames) {
        // Ownership of buf transfers to push_buffer(); synthesize a monotonic PTS/DURATION
        // since the original stream PTS is not guaranteed contiguous for this clip.
        buf = gst_buffer_make_writable(buf);
        GST_BUFFER_PTS(buf) = pts;
        GST_BUFFER_DTS(buf) = pts;
        GST_BUFFER_DURATION(buf) = duration_per_frame;
        pts += duration_per_frame;
        if (gst_app_src_push_buffer(GST_APP_SRC(appsrc), buf) != GST_FLOW_OK)
            push_failed = true;
    }
    gst_app_src_end_of_stream(GST_APP_SRC(appsrc));

    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                                 static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
    bool ok = !push_failed;
    if (msg) {
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            GError *err = nullptr;
            gchar *dbg = nullptr;
            gst_message_parse_error(msg, &err, &dbg);
            error_out = err ? err->message : "unknown pipeline error";
            if (err)
                g_error_free(err);
            g_free(dbg);
            ok = false;
        }
        gst_message_unref(msg);
    } else {
        error_out = "timed out waiting for recording pipeline to finish";
        ok = false;
    }
    gst_object_unref(bus);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(appsrc);
    gst_object_unref(sink);
    gst_object_unref(pipeline);
    return ok;
}

} // namespace gvaeventrecorder
