/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <functional>
#include <gst/gst.h>
#include <gst/gsttracer.h>
#include <map>
#include <memory>
#include <set>
#include <string>

#define ELEMENT_DESCRIPTION "Buffers tracing - gst_pad_push statistic"

template <class T>
using auto_ptr = std::unique_ptr<T, std::function<void(T *)>>;

struct BufferStatistic {
    struct ElementStatistic {
        std::set<GstBuffer *> input_buffer_pool;
        std::set<GstBuffer *> output_buffer_pool;
        int num_buffers = 0;
        double start_time = 0;
        double last_time = 0;
        double total = 0;
        std::string name;

        std::set<ElementStatistic *> next;
        bool printed = false;

        void Update(GstBuffer *buffer, double ts, int inc) {
            if (start_time == 0) {
                start_time = ts;
                last_time = ts;
            }
            if (inc > 0)
                input_buffer_pool.insert(buffer);
            if (inc < 0)
                output_buffer_pool.insert(buffer);

            total += (ts - last_time) * num_buffers;
            num_buffers += inc;
            last_time = ts;
        }

        void Print() {
            if (printed)
                return;
            std::string buffer_pool_size;
            if (output_buffer_pool.size() != input_buffer_pool.size() ||
                !equal(output_buffer_pool.begin(), output_buffer_pool.end(), input_buffer_pool.begin())) {
                buffer_pool_size = std::to_string(output_buffer_pool.size());
            }
            double average = (total > 0) ? total / (last_time - start_time) : 0;
            printf("%70s, %-7s, %-7d, %7.2f\n", name.data(), buffer_pool_size.data(), num_buffers, average);
            printed = true;

            for (auto elem : next) {
                elem->Print();
            }
        }
    };

    std::map<GstElement *, ElementStatistic> _stat;
    std::set<GstBin *> all_bins;
    double last_printing_ts = 0;

    void register_bin(GstPad *pad) {
        auto elem = auto_ptr<GstElement>(gst_pad_get_parent_element(pad), gst_object_unref);
        if (elem && GST_IS_BIN(elem.get()))
            all_bins.insert(GST_BIN(elem.get()));
    }

    static int cmp_ptr(gconstpointer a, gconstpointer b) {
        const GValue *item = (const GValue *)a;
        GstElement *elem = GST_ELEMENT(g_value_get_object(item));
        if (elem == b)
            return 0;
        else
            return 1;
    }

    std::string find_upper_bins(GstElement *element) {
        std::string str;
        for (auto bin : all_bins) {
            auto iter = gst_bin_iterate_recurse(bin);
            GValue val = {};
            if (gst_iterator_find_custom(iter, cmp_ptr, &val, (gpointer)element)) {
                auto name = gst_element_get_name(bin);
                str += std::string(name) + std::string(" / ");
                g_free(name);
            }
            g_value_unset(&val);
            gst_iterator_free(iter);
        }
        return str;
    }

    void set_name(GstElement *elem) {
        auto p = &_stat[elem];
        if (p->name.empty()) {
            auto g_name = gst_element_get_name(elem);
            p->name = find_upper_bins(elem) + std::string(g_name);
            g_free(g_name);
        }
    }

    void pad_push_event(GstClockTime clock_ts, GstPad *_pad, GstBuffer *buffer) {
        if (!_pad)
            return;
        auto pad = auto_ptr<GstPad>((GstPad *)gst_object_ref(_pad), gst_object_unref);
        double ts = clock_ts * 1e-9;

        std::map<int, GstElement *> elements;

        for (int inc = -1; inc <= 1; inc += 2) { // for [ upstream element, downstream element ]
            if (!pad)
                break;
            register_bin(pad.get());

            auto elem = auto_ptr<GstElement>(gst_pad_get_parent_element(pad.get()), gst_object_unref);
            if (elem)
                _stat[elem.get()].Update(buffer, ts, inc);

            // try go from bin element to real element
            while (GST_IS_GHOST_PAD(pad.get())) {
                pad = auto_ptr<GstPad>(gst_ghost_pad_get_target(GST_GHOST_PAD(pad.get())), gst_object_unref);
            }
            auto elem2 = std::shared_ptr<GstElement>(gst_pad_get_parent_element(pad.get()), gst_object_unref);
            if (elem2 && elem2.get() != elem.get())
                _stat[elem2.get()].Update(buffer, ts, inc);

            elements[inc] = elem2.get();

            // switch from upstream element to downstream element
            pad = auto_ptr<GstPad>(gst_pad_get_peer(_pad), gst_object_unref);
        }

        if (elements[-1] && elements[1])
            _stat[elements[-1]].next.insert(&_stat[elements[1]]);

        if (!last_printing_ts)
            last_printing_ts = ts;
        if (ts - last_printing_ts > 2) {
            printf("%70s, %-7s, %-7s, %-7s\n", "BIN NAME / ELEMENT NAME", "POOL", "BUFFERS", "AVERAGE BUFFERS");
            printf("%70s, %-7s, %-7s, %-7s\n", "------------------------------------------", "-----", "-----", "-----");
            for (auto &stat : _stat) {
                set_name(stat.first);
                stat.second.printed = false;
            }
            for (auto &stat : _stat) {
                if (stat.second.name.find("demux") != std::string::npos) {
                    stat.second.Print();
                }
            }
            for (auto &stat : _stat)
                stat.second.Print();
            last_printing_ts = ts;
        }
    }
};

extern "C" {

typedef struct _BufferTracer BufferTracer;
typedef struct _BufferTracerClass BufferTracerClass;

struct _BufferTracer {
    ~_BufferTracer() {
        stat.reset();
    }

    GstTracer parent;
    std::shared_ptr<BufferStatistic> stat;
};

struct _BufferTracerClass {
    GstTracerClass parent_class;
};

G_GNUC_INTERNAL GType buffer_tracer_get_type(void);

// GST_DEBUG_CATEGORY_STATIC(gst_itt_debug);
// #define GST_CAT_DEFAULT gst_itt_debug
// #define _do_init GST_DEBUG_CATEGORY_INIT(gst_itt_debug, "itt", 0, "itt tracer");

#define buffer_tracer_parent_class parent_class

// G_DEFINE_TYPE_WITH_CODE(BufferTracer, buffer_tracer, GST_TYPE_TRACER, _do_init);
G_DEFINE_TYPE(BufferTracer, buffer_tracer, GST_TYPE_TRACER);

static void buffer_tracer_finalize(GObject *obj) {
    // BufferTracer *self = buffer_TRACER (obj);

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void buffer_tracer_class_init(BufferTracerClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = buffer_tracer_finalize;
}

static void GstTracerHookPadPushPre(GObject *self, GstClockTime ts, GstPad *pad, GstBuffer *buffer) {
    ((BufferTracer *)self)->stat->pad_push_event(ts, pad, buffer);
}

static void buffer_tracer_init(BufferTracer *self) {
    /* Register callbacks */
    gst_tracing_register_hook(GST_TRACER(self), "pad-push-pre", G_CALLBACK(GstTracerHookPadPushPre));
    // gst_tracing_register_hook(GST_TRACER(self), "pad-push-post", G_CALLBACK(GstTracerHookPadPushPost));

    self->stat = std::make_shared<BufferStatistic>();
}

static gboolean plugin_init(GstPlugin *plugin) {
    if (!gst_tracer_register(plugin, "buffer_tracer", buffer_tracer_get_type())) {
        return FALSE;
    }
    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, buffer_tracer, ELEMENT_DESCRIPTION, plugin_init, PLUGIN_VERSION,
                  PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)

} // extern "C"
