/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <glib/gstdio.h>
#include <gst/gst.h>
#include <gst/gsttracer.h>
#include <gst/gsttracerrecord.h>
#include <ittnotify.h>
#include <string.h>

#define ELEMENT_DESCRIPTION "Performance tracing utilizing Intel ITT interface"

G_BEGIN_DECLS

#define UNUSED(x) (void)(x)

#define GST_TYPE_GVAITTTRACER (gst_gvaitttracer_get_type())
#define GST_GVAITTTRACER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVAITTTRACER, GstGvaITTTracer))

typedef struct _GstGvaITTTracer GstGvaITTTracer;
typedef struct _GstGvaITTTracerClass GstGvaITTTracerClass;

struct _GstGvaITTTracer {
    GstTracer parent;
    __itt_domain *domain;
};

struct _GstGvaITTTracerClass {
    GstTracerClass parent_class;
};

G_GNUC_INTERNAL GType gst_gvaitttracer_get_type(void);

G_END_DECLS

GST_DEBUG_CATEGORY_STATIC(gst_itt_debug);
#define GST_CAT_DEFAULT gst_itt_debug

#define _do_init GST_DEBUG_CATEGORY_INIT(gst_itt_debug, "itt", 0, "itt tracer");
#define gst_gvaitttracer_parent_class parent_class

G_DEFINE_TYPE_WITH_CODE(GstGvaITTTracer, gst_gvaitttracer, GST_TYPE_TRACER, _do_init);

static void gst_gvaitttracer_finalize(GObject *obj) {
    // GstGvaITTTracer *self = GST_GVAITTTRACER (obj);

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void gst_gvaitttracer_class_init(GstGvaITTTracerClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gst_gvaitttracer_finalize;
}

static void GstTracerHookPadPushPre(GObject *self, GstClockTime ts, GstPad *pad, GstBuffer *buffer) {
    UNUSED(ts);
    UNUSED(buffer);
    if (pad) {
        GstElement *elem = gst_pad_get_parent_element(pad);
        if (elem) {
            GstPad *pad2 = gst_pad_get_peer(pad);
            if (pad2) {
                GstElement *elem2 = gst_pad_get_parent_element(pad2);
                if (elem2) {
                    char *name2 = gst_element_get_name(elem2);
                    if (name2) {
                        __itt_task_begin(GST_GVAITTTRACER(self)->domain, __itt_null, __itt_null,
                                         __itt_string_handle_create(name2));
                        g_free(name2);
                    }
                    gst_object_unref(elem2);
                }
                gst_object_unref(pad2);
            }
            gst_object_unref(elem);
        }
    }
}

static void GstTracerHookPadPushPost(GObject *self, GstClockTime ts, GstPad *pad, GstFlowReturn res) {
    UNUSED(ts);
    UNUSED(pad);
    UNUSED(res);

    __itt_task_end(GST_GVAITTTRACER(self)->domain);
}

static void gst_gvaitttracer_init(GstGvaITTTracer *self) {
    /* Register callbacks */
    gst_tracing_register_hook(GST_TRACER(self), "pad-push-pre", G_CALLBACK(GstTracerHookPadPushPre));
    gst_tracing_register_hook(GST_TRACER(self), "pad-push-post", G_CALLBACK(GstTracerHookPadPushPost));

    self->domain = __itt_domain_create("gst-itt-tracer");
}

static gboolean plugin_init(GstPlugin *plugin) {
    if (!gst_tracer_register(plugin, "gvaitttracer", gst_gvaitttracer_get_type())) {
        return FALSE;
    }
    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, gvaitttracer, ELEMENT_DESCRIPTION, plugin_init, PLUGIN_VERSION,
                  PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
