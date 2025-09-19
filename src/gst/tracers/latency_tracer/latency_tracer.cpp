/*******************************************************************************
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "latency_tracer.h"
#include "latency_tracer_meta.h"
#include <mutex>
#include <tuple>
using namespace std;

#define ELEMENT_DESCRIPTION "Latency tracer to calculate time it takes to process each frame for element and pipeline"
GST_DEBUG_CATEGORY_STATIC(latency_tracer_debug);
G_DEFINE_TYPE(LatencyTracer, latency_tracer, GST_TYPE_TRACER);

static GstTracerRecord *tr_pipeline;
static GstTracerRecord *tr_element;
static GstTracerRecord *tr_element_interval;
static GstTracerRecord *tr_pipeline_interval;
static guint ns_to_ms = 1000000;
static guint ms_to_s = 1000;
using BufferListArgs = tuple<LatencyTracer *, guint64, GstPad *>;
#define UNUSED(x) (void)(x)

static GQuark data_string = g_quark_from_static_string("latency_tracer");

static void latency_tracer_constructed(GObject *object) {
    LatencyTracer *lt = LATENCY_TRACER(object);
    gchar *params, *tmp;
    GstStructure *params_struct = NULL;
    g_object_get(lt, "params", &params, NULL);
    if (!params)
        return;

    tmp = g_strdup_printf("latency_tracer,%s", params);
    params_struct = gst_structure_from_string(tmp, NULL);
    g_free(tmp);

    if (params_struct) {
        const gchar *flags;
        /* Read the flags if available */
        flags = gst_structure_get_string(params_struct, "flags");
        if (flags) {
            lt->flags = static_cast<LatencyTracerFlags>(0);
            GStrv split = g_strsplit(flags, "+", -1);
            for (gint i = 0; split[i]; i++) {
                if (g_str_equal(split[i], "pipeline"))
                    lt->flags = static_cast<LatencyTracerFlags>(lt->flags | LATENCY_TRACER_FLAG_PIPELINE);
                else if (g_str_equal(split[i], "element"))
                    lt->flags = static_cast<LatencyTracerFlags>(lt->flags | LATENCY_TRACER_FLAG_ELEMENT);
                else
                    GST_WARNING_OBJECT(lt, "Invalid latency tracer flags %s", split[i]);
            }
            g_strfreev(split);
        }
        gst_structure_get_int(params_struct, "interval", &lt->interval);
        GST_INFO_OBJECT(lt, "interval set to %d ms", lt->interval);
        gst_structure_free(params_struct);
    }
    g_free(params);
}

static void latency_tracer_class_init(LatencyTracerClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->constructed = latency_tracer_constructed;
    tr_pipeline = gst_tracer_record_new(
        "latency_tracer_pipeline.class", "frame_latency", GST_TYPE_STRUCTURE,
        gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description", G_TYPE_STRING,
                          "current frame latency in ms", NULL),
        "avg", GST_TYPE_STRUCTURE,
        gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description", G_TYPE_STRING,
                          "Average frame latency in ms", NULL),
        "min", GST_TYPE_STRUCTURE,
        gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description", G_TYPE_STRING,
                          "Min Per frame latency in ms", NULL),
        "max", GST_TYPE_STRUCTURE,
        gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description", G_TYPE_STRING,
                          "Max Per frame latency in ms", NULL),
        "latency", GST_TYPE_STRUCTURE,
        gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description", G_TYPE_STRING,
                          "pipeline latency in ms(if frames dropped this may result in invalid value)", NULL),
        "fps", GST_TYPE_STRUCTURE,
        gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description", G_TYPE_STRING,
                          "pipeline fps(if frames dropped this may result in invalid value)", NULL),
        "frame_num", GST_TYPE_STRUCTURE,
        gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_UINT, "description", G_TYPE_STRING,
                          "NUmber of frame processed", NULL),
        NULL);

    tr_pipeline_interval = gst_tracer_record_new(
        "latency_tracer_pipeline_interval.class", "interval", GST_TYPE_STRUCTURE,
        gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description", G_TYPE_STRING, "interval in ms",
                          NULL),
        "avg", GST_TYPE_STRUCTURE,
        gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description", G_TYPE_STRING,
                          "Average interval frame latency in ms", NULL),
        "min", GST_TYPE_STRUCTURE,
        gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description", G_TYPE_STRING,
                          "Min interval Per frame latency in ms", NULL),
        "max", GST_TYPE_STRUCTURE,
        gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description", G_TYPE_STRING,
                          "Max interval Per frame latency in ms", NULL),
        "latency", GST_TYPE_STRUCTURE,
        gst_structure_new(
            "value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description", G_TYPE_STRING,
            "pipeline latency within the interval in ms(if frames dropped this may result in invalid value)", NULL),
        "fps", GST_TYPE_STRUCTURE,
        gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description", G_TYPE_STRING,
                          "pipeline fps ithin the interval(if frames dropped this may result in invalid value)", NULL),
        NULL);
    tr_element = gst_tracer_record_new("latency_tracer_element.class", "name", GST_TYPE_STRUCTURE,
                                       gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_STRING, "description",
                                                         G_TYPE_STRING, "Element Name", NULL),
                                       "frame_latency", GST_TYPE_STRUCTURE,
                                       gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description",
                                                         G_TYPE_STRING, "current frame latency in ms", NULL),
                                       "avg", GST_TYPE_STRUCTURE,
                                       gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description",
                                                         G_TYPE_STRING, "Average frame latency in ms", NULL),
                                       "min", GST_TYPE_STRUCTURE,
                                       gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description",
                                                         G_TYPE_STRING, "Min Per frame latency in ms", NULL),
                                       "max", GST_TYPE_STRUCTURE,
                                       gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description",
                                                         G_TYPE_STRING, "Max Per frame latency in ms", NULL),
                                       "frame_num", GST_TYPE_STRUCTURE,
                                       gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_UINT, "description",
                                                         G_TYPE_STRING, "Number of frame processed", NULL),
                                       "is_bin", GST_TYPE_STRUCTURE,
                                       gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_BOOLEAN, "description",
                                                         G_TYPE_STRING, "is element bin", NULL),
                                       NULL);
    tr_element_interval =
        gst_tracer_record_new("latency_tracer_element_interval.class", "name", GST_TYPE_STRUCTURE,
                              gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_STRING, "description",
                                                G_TYPE_STRING, "Element Name", NULL),
                              "interval", GST_TYPE_STRUCTURE,
                              gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description",
                                                G_TYPE_STRING, "Interval ms", NULL),
                              "avg", GST_TYPE_STRUCTURE,
                              gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description",
                                                G_TYPE_STRING, "Average interval latency in ms", NULL),
                              "min", GST_TYPE_STRUCTURE,
                              gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description",
                                                G_TYPE_STRING, "Min interval frame latency in ms", NULL),
                              "max", GST_TYPE_STRUCTURE,
                              gst_structure_new("value", "type", G_TYPE_GTYPE, G_TYPE_DOUBLE, "description",
                                                G_TYPE_STRING, "Max interval frame latency in ms", NULL),
                              NULL);
    GST_DEBUG_CATEGORY_INIT(latency_tracer_debug, "latency_tracer", 0, "latency tracer");
}

static GstElement *get_real_pad_parent(GstPad *pad) {
    GstObject *parent;
    if (!pad)
        return NULL;
    parent = gst_object_get_parent(GST_OBJECT_CAST(pad));
    /* if parent of pad is a ghost-pad, then pad is a proxy_pad */
    if (parent && GST_IS_GHOST_PAD(parent)) {
        GstObject *tmp;
        pad = GST_PAD_CAST(parent);
        tmp = gst_object_get_parent(GST_OBJECT_CAST(pad));
        gst_object_unref(parent);
        parent = tmp;
    }
    return GST_ELEMENT_CAST(parent);
}

struct ElementStats {
    gboolean is_bin;
    gdouble total;
    gdouble min;
    gdouble max;
    guint frame_count;
    gchar *name;
    gdouble interval_total;
    gdouble interval_min;
    gdouble interval_max;
    guint interval_frame_count;
    GstClockTime interval_init_time;
    mutex mtx;

    static void create(GstElement *elem, guint64 ts) {
        // This won't be converted to shared ptr because g_object_set_qdata_full destructor supports gpointer only
        auto *stats = new ElementStats{elem, ts};
        g_object_set_qdata_full(reinterpret_cast<GObject *>(elem), data_string, stats,
                                [](gpointer data) { delete static_cast<ElementStats *>(data); });
    }

    static ElementStats *from_element(GstElement *elem) {
        if (!elem)
            return nullptr;
        return static_cast<ElementStats *>(g_object_get_qdata(G_OBJECT(elem), data_string));
    }

    ElementStats(GstElement *elem, GstClockTime ts) {
        is_bin = GST_IS_BIN(elem);
        total = 0;
        min = G_MAXUINT;
        max = 0;
        frame_count = 0;
        name = GST_ELEMENT_NAME(elem);
        reset_interval(ts);
    }

    void reset_interval(GstClockTime now) {
        interval_total = 0;
        interval_min = G_MAXUINT;
        interval_max = 0;
        interval_init_time = now;
        interval_frame_count = 0;
    }

    void cal_log_element_latency(guint64 src_ts, guint64 sink_ts, gint interval) {
        lock_guard<mutex> guard(mtx);
        frame_count += 1;
        gdouble frame_latency = (gdouble)GST_CLOCK_DIFF(sink_ts, src_ts) / ns_to_ms;
        total += frame_latency;
        gdouble avg = total / frame_count;
        if (frame_latency < min)
            min = frame_latency;
        if (frame_latency > max)
            max = frame_latency;
        gst_tracer_record_log(tr_element, name, frame_latency, avg, min, max, frame_count, is_bin);
        cal_log_interval(frame_latency, src_ts, interval);
    }

    void cal_log_interval(gdouble frame_latency, guint64 src_ts, gint interval) {
        interval_frame_count += 1;
        interval_total += frame_latency;
        if (frame_latency < interval_min)
            interval_min = frame_latency;
        if (frame_latency > interval_max)
            interval_max = frame_latency;
        gdouble ms = (gdouble)GST_CLOCK_DIFF(interval_init_time, src_ts) / ns_to_ms;
        if (ms >= interval) {
            gdouble interval_avg = interval_total / interval_frame_count;
            gst_tracer_record_log(tr_element_interval, name, ms, interval_avg, interval_min, interval_max);
            reset_interval(src_ts);
        }
    }
};

static bool is_parent_pipeline(LatencyTracer *lt, GstElement *elem) {
    GstElement *parent_elm = GST_ELEMENT_PARENT(elem);
    if (parent_elm != lt->pipeline)
        return false;
    return true;
}

static void reset_pipeline_interval(LatencyTracer *lt, GstClockTime now) {
    lt->interval_total = 0;
    lt->interval_min = G_MAXUINT;
    lt->interval_max = 0;
    lt->interval_init_time = now;
    lt->interval_frame_count = 0;
}

static void cal_log_pipeline_interval(LatencyTracer *lt, guint64 ts, gdouble frame_latency) {
    lt->interval_frame_count += 1;
    lt->interval_total += frame_latency;
    if (frame_latency < lt->interval_min)
        lt->interval_min = frame_latency;
    if (frame_latency > lt->interval_max)
        lt->interval_max = frame_latency;
    gdouble ms = (gdouble)GST_CLOCK_DIFF(lt->interval_init_time, ts) / ns_to_ms;
    if (ms >= lt->interval) {
        gdouble pipeline_latency = ms / lt->interval_frame_count;
        gdouble fps = ms_to_s / pipeline_latency;
        gdouble interval_avg = lt->interval_total / lt->interval_frame_count;
        gst_tracer_record_log(tr_pipeline_interval, ms, interval_avg, lt->interval_min, lt->interval_max,
                              pipeline_latency, fps);
        reset_pipeline_interval(lt, ts);
    }
}

static void cal_log_pipeline_latency(LatencyTracer *lt, guint64 ts, LatencyTracerMeta *meta) {
    GST_OBJECT_LOCK(lt);
    lt->frame_count += 1;
    gdouble frame_latency = (gdouble)GST_CLOCK_DIFF(meta->init_ts, ts) / ns_to_ms;
    gdouble pipeline_latency_ns = (gdouble)GST_CLOCK_DIFF(lt->first_frame_init_ts, ts) / lt->frame_count;
    gdouble pipeline_latency = pipeline_latency_ns / ns_to_ms;
    lt->toal_latency += frame_latency;
    gdouble avg = lt->toal_latency / lt->frame_count;
    gdouble fps = 0;
    if (pipeline_latency > 0)
        fps = ms_to_s / pipeline_latency;

    if (frame_latency < lt->min)
        lt->min = frame_latency;
    if (frame_latency > lt->max)
        lt->max = frame_latency;

    gst_tracer_record_log(tr_pipeline, frame_latency, avg, lt->min, lt->max, pipeline_latency, fps, lt->frame_count);
    cal_log_pipeline_interval(lt, ts, frame_latency);
    GST_OBJECT_UNLOCK(lt);
}

static void add_latency_meta(LatencyTracer *lt, LatencyTracerMeta *meta, guint64 ts, GstBuffer *buffer,
                             GstElement *elem) {
    if (!gst_buffer_is_writable(buffer)) {
        GST_ERROR_OBJECT(lt, "buffer not writable, unable to add LatencyTracerMeta at element=%s, ts=%ld, buffer=%p",
                         GST_ELEMENT_NAME(elem), ts, buffer);
        return;
    }
    meta = LATENCY_TRACER_META_ADD(buffer);
    meta->init_ts = ts;
    meta->last_pad_push_ts = ts;
    if (lt->first_frame_init_ts == 0) {
        reset_pipeline_interval(lt, ts);
        lt->first_frame_init_ts = ts;
    }
}

static void do_push_buffer_pre(LatencyTracer *lt, guint64 ts, GstPad *pad, GstBuffer *buffer) {
    GstElement *elem = get_real_pad_parent(pad);
    if (!is_parent_pipeline(lt, elem))
        return;
    LatencyTracerMeta *meta = LATENCY_TRACER_META_GET(buffer);
    if (!meta) {
        add_latency_meta(lt, meta, ts, buffer, elem);
        return;
    }
    if (lt->flags & LATENCY_TRACER_FLAG_ELEMENT) {
        ElementStats *stats = ElementStats::from_element(elem);
        // log latency only if ts is greater than last logged ts to avoid duplicate logging for the same buffer
        if (stats != nullptr && ts > meta->last_pad_push_ts) {
            stats->cal_log_element_latency(ts, meta->last_pad_push_ts, lt->interval);
            meta->last_pad_push_ts = ts;
        }
    }
    if (lt->flags & LATENCY_TRACER_FLAG_PIPELINE && lt->sink_element == get_real_pad_parent(GST_PAD_PEER(pad))) {
        cal_log_pipeline_latency(lt, ts, meta);
    }
}

static void do_pull_range_post(LatencyTracer *lt, guint64 ts, GstPad *pad, GstBuffer *buffer) {
    GstElement *elem = get_real_pad_parent(pad);
    if (!is_parent_pipeline(lt, elem))
        return;
    LatencyTracerMeta *meta = nullptr;
    add_latency_meta(lt, meta, ts, buffer, elem);
}

static void do_push_buffer_list_pre(LatencyTracer *lt, guint64 ts, GstPad *pad, GstBufferList *list) {
    BufferListArgs args{lt, ts, pad};
    gst_buffer_list_foreach(
        list,
        [](GstBuffer **buffer, guint, gpointer user_data) -> gboolean {
            auto [lt, ts, pad] = *static_cast<BufferListArgs *>(user_data);
            do_push_buffer_pre(lt, ts, pad, *buffer);
            return TRUE;
        },
        &args);
}

static void on_element_change_state_post(LatencyTracer *lt, guint64 ts, GstElement *elem, GstStateChange change,
                                         GstStateChangeReturn result) {
    UNUSED(result);
    if (GST_STATE_TRANSITION_NEXT(change) == GST_STATE_PLAYING && elem == lt->pipeline) {
        GstIterator *iter = gst_bin_iterate_elements(GST_BIN_CAST(elem));
        while (true) {
            GValue gval = {};
            auto ret = gst_iterator_next(iter, &gval);
            if (ret != GST_ITERATOR_OK) {
                if (ret != GST_ITERATOR_DONE)
                    GST_ERROR_OBJECT(lt, "Got error while iterating pipeline");
                break;
            }
            auto *element = static_cast<GstElement *>(g_value_get_object(&gval));
            GST_INFO_OBJECT(lt, "Element %s ", GST_ELEMENT_NAME(element));
            if (GST_OBJECT_FLAG_IS_SET(element, GST_ELEMENT_FLAG_SINK))
                lt->sink_element = element;
            else if (!GST_OBJECT_FLAG_IS_SET(element, GST_ELEMENT_FLAG_SOURCE)) {
                // create ElementStats only once per each element
                if (!ElementStats::from_element(element)) {
                    ElementStats::create(element, ts);
                }
            }
        }
        GstTracer *tracer = GST_TRACER(lt);
        gst_tracing_register_hook(tracer, "pad-push-pre", G_CALLBACK(do_push_buffer_pre));
        gst_tracing_register_hook(tracer, "pad-push-list-pre", G_CALLBACK(do_push_buffer_list_pre));
        gst_tracing_register_hook(tracer, "pad-pull-range-post", G_CALLBACK(do_pull_range_post));
    }
}
static void on_element_new(LatencyTracer *lt, guint64 ts, GstElement *elem) {
    UNUSED(ts);
    if (GST_IS_PIPELINE(elem)) {
        if (!lt->pipeline)
            lt->pipeline = elem;
        else
            GST_WARNING_OBJECT(lt, "pipeline %s already exists, multiple pipelines may not give right result %s",
                               GST_ELEMENT_NAME(lt->pipeline), GST_ELEMENT_NAME(elem));
    }
}

static void latency_tracer_init(LatencyTracer *lt) {
    GST_OBJECT_LOCK(lt);
    lt->toal_latency = 0;
    lt->frame_count = 0;
    lt->first_frame_init_ts = 0;
    lt->pipeline = nullptr;
    lt->sink_element = nullptr;
    lt->min = G_MAXUINT;
    lt->max = 0;
    lt->flags = static_cast<LatencyTracerFlags>(LATENCY_TRACER_FLAG_ELEMENT | LATENCY_TRACER_FLAG_PIPELINE);
    lt->interval = 1000;

    GstTracer *tracer = GST_TRACER(lt);
    gst_tracing_register_hook(tracer, "element-new", G_CALLBACK(on_element_new));
    gst_tracing_register_hook(tracer, "element-change-state-post", G_CALLBACK(on_element_change_state_post));
    GST_OBJECT_UNLOCK(lt);
}

static gboolean plugin_init(GstPlugin *plugin) {
    if (!gst_tracer_register(plugin, "latency_tracer", latency_tracer_get_type()))
        return false;
    latency_tracer_meta_get_info();
    latency_tracer_meta_api_get_type();
    return true;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, latency_tracer, ELEMENT_DESCRIPTION, plugin_init,
                  PLUGIN_VERSION, PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
