/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "processbin.h"

GST_DEBUG_CATEGORY_STATIC(processbin_debug);
#define GST_CAT_DEFAULT processbin_debug

enum {
    PROP_0,
    PROP_PREPROCESS,
    PROP_PROCESS,
    PROP_POSTPROCESS,
    PROP_AGGREGATE,
    PROP_POSTAGGREGATE,
    PROP_PREPROCESS_QUEUE_SIZE,
    PROP_PROCESS_QUEUE_SIZE,
    PROP_POSTPROCESS_QUEUE_SIZE,
    PROP_AGGREGATE_QUEUE_SIZE,
    PROP_POSTAGGREGATE_QUEUE_SIZE,
    PROP_LAST
};

#define DEFAULT_QUEUE_SIZE 0 // unlimited

#define RETURN_IF_FALSE(_VALUE)                                                                                        \
    if (!(_VALUE)) {                                                                                                   \
        GST_ERROR_OBJECT(self, #_VALUE " is 0");                                                                       \
        return FALSE;                                                                                                  \
    }

static GstBinClass *parent_class = NULL;

static void processbin_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void processbin_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static GstStateChangeReturn processbin_change_state(GstElement *element, GstStateChange transition);
static void processbin_dispose(GObject *object);

static void processbin_class_init(GstProcessBinClass *klass) {
    GObjectClass *gobject_klass = G_OBJECT_CLASS(klass);
    GstElementClass *gstelement_klass = GST_ELEMENT_CLASS(klass);
    GParamFlags flags = G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT;

    parent_class = g_type_class_peek_parent(klass);

    gobject_klass->set_property = processbin_set_property;
    gobject_klass->get_property = processbin_get_property;
    gobject_klass->dispose = processbin_dispose;
    gstelement_klass->change_state = processbin_change_state;

    g_object_class_install_property(
        gobject_klass, PROP_PREPROCESS,
        g_param_spec_object("preprocess", "preprocess", "Pre-processing element", GST_TYPE_ELEMENT, flags));
    g_object_class_install_property(
        gobject_klass, PROP_PROCESS,
        g_param_spec_object("process", "process", "Main processing element", GST_TYPE_ELEMENT, flags));
    g_object_class_install_property(
        gobject_klass, PROP_POSTPROCESS,
        g_param_spec_object("postprocess", "postprocess", "Post-processing element", GST_TYPE_ELEMENT, flags));
    g_object_class_install_property(
        gobject_klass, PROP_AGGREGATE,
        g_param_spec_object("aggregate", "aggregate",
                            "(Optional) Element to aggregate preprocess/process/postprocess result and original frame",
                            GST_TYPE_ELEMENT, flags));
    g_object_class_install_property(gobject_klass, PROP_POSTAGGREGATE,
                                    g_param_spec_object("postaggregate", "postaggregate",
                                                        "(Optional) Element inserted after aggregation element",
                                                        GST_TYPE_ELEMENT, flags));

    g_object_class_install_property(
        gobject_klass, PROP_PREPROCESS_QUEUE_SIZE,
        g_param_spec_int("preprocess-queue-size", "preprocess-queue-size",
                         "Size of queue (in number buffers) before pre-processing element. "
                         "Special values: -1 means no queue element, 0 means queue of unlimited size",
                         -1, INT_MAX, DEFAULT_QUEUE_SIZE, flags));
    g_object_class_install_property(
        gobject_klass, PROP_PROCESS_QUEUE_SIZE,
        g_param_spec_int("process-queue-size", "process-queue-size",
                         "Size of queue (in number buffers) before processing element. "
                         "Special values: -1 means no queue element, 0 means queue of unlimited size",
                         -1, INT_MAX, DEFAULT_QUEUE_SIZE, flags));
    g_object_class_install_property(
        gobject_klass, PROP_POSTPROCESS_QUEUE_SIZE,
        g_param_spec_int("postprocess-queue-size", "postprocess-queue-size",
                         "Size of queue (in number buffers) before post-processing element. "
                         "Special values: -1 means no queue element, 0 means queue of unlimited size",
                         -1, INT_MAX, DEFAULT_QUEUE_SIZE, flags));
    g_object_class_install_property(
        gobject_klass, PROP_AGGREGATE_QUEUE_SIZE,
        g_param_spec_int("aggregate-queue-size", "aggregate-queue-size",
                         "Size of queue (in number buffers) for original frames between 'tee' and aggregate element. "
                         "Special values: -1 means no queue element, 0 means queue of unlimited size",
                         -1, INT_MAX, DEFAULT_QUEUE_SIZE, flags));
    g_object_class_install_property(
        gobject_klass, PROP_POSTAGGREGATE_QUEUE_SIZE,
        g_param_spec_int("postaggregate-queue-size", "postaggregate-queue-size",
                         "Size of queue (in number buffers) between aggregate and post-aggregate elements. "
                         "Special values: -1 means no queue element, 0 means queue of unlimited size",
                         -1, INT_MAX, DEFAULT_QUEUE_SIZE, flags));

    /* pad templates */
    static GstStaticPadTemplate sink_template =
        GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);
    static GstStaticPadTemplate src_template =
        GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);
    gst_element_class_add_static_pad_template(gstelement_klass, &sink_template);
    gst_element_class_add_static_pad_template(gstelement_klass, &src_template);

    gst_element_class_set_static_metadata(
        gstelement_klass, "Generic process bin element", "Generic process bin element",
        "Bin element for processing pipelines using branching: "
        "tee name=t t. ! <preprocess> ! <process> ! <postprocess> ! <aggregate>  t. ! aggregate",
        "Intel Corporation");
}

gboolean processbin_sink_pad_event_function(GstPad *pad, GstObject *parent, GstEvent *event);

static void processbin_init(GstProcessBin *self) {
    self->preprocess = NULL;
    self->process = NULL;
    self->postprocess = NULL;
    self->aggregate = NULL;
    self->postaggregate = NULL;
    self->preprocess_queue_size = -1;
    self->process_queue_size = -1;
    self->postprocess_queue_size = -1;
    self->aggregate_queue_size = -1;
    self->postaggregate_queue_size = -1;

    self->sink_pad = gst_ghost_pad_new_no_target("sink", GST_PAD_SINK);
    gst_element_add_pad(GST_ELEMENT(self), self->sink_pad);

    self->src_pad = gst_ghost_pad_new_no_target("src", GST_PAD_SRC);
    gst_element_add_pad(GST_ELEMENT(self), self->src_pad);

    gst_pad_set_event_function(self->sink_pad, processbin_sink_pad_event_function);

    // initially bin constructed in passthrough mode (as element 'identity')
    self->identity = gst_element_factory_make("identity", NULL);
    gst_bin_add(GST_BIN(self), self->identity);
    GstPad *sink_pad = gst_element_get_static_pad(self->identity, "sink");
    GstPad *src_pad = gst_element_get_static_pad(self->identity, "src");
    gst_ghost_pad_set_target(GST_GHOST_PAD(self->sink_pad), sink_pad);
    gst_object_unref(sink_pad);
    gst_ghost_pad_set_target(GST_GHOST_PAD(self->src_pad), src_pad);
    gst_object_unref(src_pad);
}

static gboolean link_via_queue(GstBin *self, GstElement *element1, GstElement *element2, gint queue_size,
                               const gchar *queue_name) {
    if (queue_size >= 0) {
        GstElement *queue = gst_element_factory_make("queue", queue_name);
        RETURN_IF_FALSE(queue);
        // disable limitation of queue size in bytes
        g_object_set(G_OBJECT(queue), "max-size-bytes", (guint)0, NULL);
        g_object_set(G_OBJECT(queue), "max-size-time", (guint64)0, NULL);
        // set limitation of queue size in number buffers. O means no limit
        g_object_set(G_OBJECT(queue), "max-size-buffers", queue_size, NULL);
        RETURN_IF_FALSE(gst_bin_add(self, queue));
        // RETURN_IF_FALSE(gst_element_link_many(element1, queue, element2, NULL));
        RETURN_IF_FALSE(gst_element_link_many(queue, element2, NULL));
        RETURN_IF_FALSE(gst_element_link_many(element1, queue, NULL));
    } else {
        RETURN_IF_FALSE(gst_element_link_many(element1, element2, NULL));
    }
    return TRUE;
}

gboolean processbin_is_linked(GstProcessBin *self) {
    return self->identity == NULL; // identity element removed after real elements linked
}

gboolean processbin_link_elements(GstProcessBin *self) {
    GstBin *bin = GST_BIN(self);
    GstPad *sink_pad = NULL;
    GstPad *src_pad = NULL;

    if (processbin_is_linked(self))
        return TRUE;

    if (self->preprocess == NULL || self->process == NULL || self->postprocess == NULL) {
        if (self->preprocess == NULL && self->process == NULL && self->postprocess == NULL && self->aggregate == NULL &&
            self->postaggregate != NULL) {
            gst_bin_add(GST_BIN(self), self->postaggregate);
            sink_pad = gst_element_get_static_pad(self->postaggregate, "sink");
            src_pad = gst_element_get_static_pad(self->postaggregate, "src");
        } else {
            // derived class may delay creating some elements until GST_EVENT_STREAM_START or GST_EVENT_CAPS
            return FALSE;
        }
    } else {
        // add to bin
        RETURN_IF_FALSE(gst_bin_add(bin, self->preprocess));
        RETURN_IF_FALSE(gst_bin_add(bin, self->process));
        RETURN_IF_FALSE(gst_bin_add(bin, self->postprocess));

        // Link preprocess -> process -> postprocess (with queue between elements if queue size != 0)
        RETURN_IF_FALSE(
            link_via_queue(bin, self->preprocess, self->process, self->process_queue_size, "process-queue"));
        RETURN_IF_FALSE(
            link_via_queue(bin, self->process, self->postprocess, self->postprocess_queue_size, "postprocess-queue"));
        //{
        //    GstPad *pad1 = gst_element_get_static_pad(self->postprocess, "src");
        //    //gst_element_get_request_pad()
        //    //GstPad *pad2 = self->aggregate->sinkpads->next->data; // second pad of aggregate element
        //    GstPad *pad2 = gst_element_get_compatible_pad(self->aggregate, pad1, NULL);
        //    //GstPad *pad2 = gst_element_get_request_pad(self->aggregate, "tensor_%u");
        //    if (gst_pad_link(pad1, pad2) != GST_PAD_LINK_OK)
        //        GST_ERROR_OBJECT(self, "Could not link elements");
        //}

        if (self->aggregate) {
            RETURN_IF_FALSE(gst_bin_add(bin, self->aggregate));

            // Create tee
            GstElement *tee = gst_element_factory_make("tee", "tee");
            RETURN_IF_FALSE(tee);
            RETURN_IF_FALSE(gst_bin_add(bin, tee));

            // tee to preprocess
            RETURN_IF_FALSE(
                link_via_queue(bin, tee, self->preprocess, self->preprocess_queue_size, "preprocess-queue"));

            // postprocess to aggregate
            const gchar *pad_name = "tensor_%u"; // TODO avoid using hardcoded pad name "tensor_%u"
            RETURN_IF_FALSE(gst_element_link_pads(self->postprocess, "src", self->aggregate, pad_name));

            // tee directly to aggregate
            RETURN_IF_FALSE(link_via_queue(bin, tee, self->aggregate, self->aggregate_queue_size, "aggregate-queue"));

            if (self->postaggregate) {
                RETURN_IF_FALSE(gst_bin_add(bin, self->postaggregate));
                RETURN_IF_FALSE(link_via_queue(bin, self->aggregate, self->postaggregate,
                                               self->postaggregate_queue_size, "postaggregate-queue"));
                RETURN_IF_FALSE(src_pad = gst_element_get_static_pad(self->postaggregate, "src"));
            } else {
                RETURN_IF_FALSE(src_pad = gst_element_get_static_pad(self->aggregate, "src"));
            }

            // ghost sink_pad
            RETURN_IF_FALSE(sink_pad = gst_element_get_static_pad(tee, "sink"));
        } else {
            // pads for ghost pads
            RETURN_IF_FALSE(sink_pad = gst_element_get_static_pad(self->preprocess, "sink"));
            RETURN_IF_FALSE(src_pad = gst_element_get_static_pad(self->postprocess, "src"));
        }
    }

    RETURN_IF_FALSE(sink_pad);
    RETURN_IF_FALSE(gst_ghost_pad_set_target(GST_GHOST_PAD(self->sink_pad), sink_pad));
    gst_object_unref(sink_pad);

    RETURN_IF_FALSE(src_pad);
    RETURN_IF_FALSE(gst_ghost_pad_set_target(GST_GHOST_PAD(self->src_pad), src_pad));
    gst_object_unref(src_pad);

    // remove 'identity'
    gst_element_set_state(self->identity, GST_STATE_NULL);
    RETURN_IF_FALSE(gst_bin_remove(bin, self->identity));
    self->identity = NULL;

    // sync all elements to current state of bin element
    RETURN_IF_FALSE(gst_bin_sync_children_states(&self->bin));

    return TRUE;
}

static GstStateChangeReturn processbin_change_state(GstElement *element, GstStateChange transition) {
    GstProcessBin *self = GST_PROCESSBIN(element);

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
    case GST_STATE_CHANGE_READY_TO_PAUSED:
        // try link elements
        processbin_link_elements(self);
        break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
        // if elements not linked at this stage, it's error
        if (!processbin_link_elements(self)) {
            GST_WARNING_OBJECT(self, "Failed to link elements");
            // return GST_STATE_CHANGE_FAILURE;
        }
        break;
    default:
        break;
    }

    return GST_CALL_PARENT_WITH_DEFAULT(GST_ELEMENT_CLASS, change_state, (element, transition),
                                        GST_STATE_CHANGE_SUCCESS);
}

gboolean processbin_sink_pad_event_function(GstPad *pad, GstObject *parent, GstEvent *event) {
    GstProcessBin *self = GST_PROCESSBIN(parent);
    int event_type = GST_EVENT_TYPE(event);

    // try link elements
    if (event_type == GST_EVENT_STREAM_START || event_type == GST_EVENT_CAPS) {
        processbin_link_elements(self);
    }

    return gst_pad_event_default(pad, parent, event);
}

static void processbin_dispose(GObject *object) {
    G_OBJECT_CLASS(parent_class)->dispose(object);
}

gboolean processbin_set_elements(GstProcessBin *self, GstElement *preprocess, GstElement *process,
                                 GstElement *postprocess, GstElement *aggregate, GstElement *postaggregate) {
    if (preprocess)
        self->preprocess = preprocess;
    if (process)
        self->process = process;
    if (postprocess)
        self->postprocess = postprocess;
    if (aggregate)
        self->aggregate = aggregate;
    if (postaggregate)
        self->postaggregate = postaggregate;

    return processbin_link_elements(self);
}

static GstElement *create_element_from_description(GstProcessBin *self, const gchar *description) {
    GstElement *element;
    GError *error = NULL;
    if (strchr(description, '!')) { // multiple elements created as bin
        element = gst_parse_bin_from_description(description, TRUE, &error);
    } else { // single element created without bin
        element = gst_parse_launch(description, &error);
    }
    if (!element) {
        const gchar *msg = (error && error->message) ? error->message : "Unknown";
        GST_ERROR_OBJECT(self, "Error creating bin element '%s': %s", description, msg);
        if (error)
            g_error_free(error);
    }
    return element;
}

gboolean processbin_set_elements_description(GstProcessBin *self, const gchar *preprocess, const gchar *process,
                                             const gchar *postprocess, const gchar *aggregate,
                                             const gchar *postaggregate) {
    if (preprocess && *preprocess) {
        GST_WARNING_OBJECT(self, "preprocess='%s'", preprocess);
        RETURN_IF_FALSE(self->preprocess = create_element_from_description(self, preprocess));
    }

    if (process && *process) {
        GST_WARNING_OBJECT(self, "process='%s'", process);
        RETURN_IF_FALSE(self->process = create_element_from_description(self, process));
    }

    if (postprocess && *postprocess) {
        GST_WARNING_OBJECT(self, "postprocess='%s'\n", postprocess);
        RETURN_IF_FALSE(self->postprocess = create_element_from_description(self, postprocess));
    }

    if (aggregate && *aggregate) {
        GST_WARNING_OBJECT(self, "aggregate='%s'\n", aggregate);
        RETURN_IF_FALSE(self->aggregate = create_element_from_description(self, aggregate));
    }

    if (postaggregate && *postaggregate) {
        GST_WARNING_OBJECT(self, "postaggregate='%s'\n", postaggregate);
        RETURN_IF_FALSE(self->postaggregate = create_element_from_description(self, postaggregate));
    }

    return processbin_link_elements(self);
}

void processbin_set_queue_size(GstProcessBin *self, int preprocess_queue_size, int process_queue_size,
                               int postprocess_queue_size, int aggregate_queue_size, int postaggregate_queue_size) {
    // Not overwrite if set already
    if (self->preprocess_queue_size == DEFAULT_QUEUE_SIZE)
        self->preprocess_queue_size = preprocess_queue_size;
    if (self->process_queue_size == DEFAULT_QUEUE_SIZE)
        self->process_queue_size = process_queue_size;
    if (self->postprocess_queue_size == DEFAULT_QUEUE_SIZE)
        self->postprocess_queue_size = postprocess_queue_size;
    if (self->aggregate_queue_size == DEFAULT_QUEUE_SIZE)
        self->aggregate_queue_size = aggregate_queue_size;
    if (self->postaggregate_queue_size == DEFAULT_QUEUE_SIZE)
        self->postaggregate_queue_size = postaggregate_queue_size;
}

static void processbin_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GstProcessBin *self = GST_PROCESSBIN(object);

    if (GST_STATE(self) != GST_STATE_NULL) {
        GST_WARNING("Can't set GstElement property if not on NULL state");
        return;
    }

    switch (prop_id) {
    case PROP_PREPROCESS:
        self->preprocess = (GstElement *)g_value_get_object(value);
        break;
    case PROP_PROCESS:
        self->process = (GstElement *)g_value_get_object(value);
        break;
    case PROP_POSTPROCESS:
        self->postprocess = (GstElement *)g_value_get_object(value);
        break;
    case PROP_AGGREGATE:
        self->aggregate = (GstElement *)g_value_get_object(value);
        break;
    case PROP_POSTAGGREGATE:
        self->postaggregate = (GstElement *)g_value_get_object(value);
        break;
    case PROP_PREPROCESS_QUEUE_SIZE:
        self->preprocess_queue_size = g_value_get_int(value);
        break;
    case PROP_PROCESS_QUEUE_SIZE:
        self->process_queue_size = g_value_get_int(value);
        break;
    case PROP_POSTPROCESS_QUEUE_SIZE:
        self->postprocess_queue_size = g_value_get_int(value);
        break;
    case PROP_AGGREGATE_QUEUE_SIZE:
        self->aggregate_queue_size = g_value_get_int(value);
        break;
    case PROP_POSTAGGREGATE_QUEUE_SIZE:
        self->postaggregate_queue_size = g_value_get_int(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void processbin_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstProcessBin *self = GST_PROCESSBIN(object);

    switch (prop_id) {
    case PROP_PREPROCESS:
        g_value_set_object(value, self->preprocess);
        break;
    case PROP_PROCESS:
        g_value_set_object(value, self->process);
        break;
    case PROP_POSTPROCESS:
        g_value_set_object(value, self->postprocess);
        break;
    case PROP_AGGREGATE:
        g_value_set_object(value, self->aggregate);
        break;
    case PROP_POSTAGGREGATE:
        g_value_set_object(value, self->postaggregate);
        break;
    case PROP_PREPROCESS_QUEUE_SIZE:
        g_value_set_int(value, self->preprocess_queue_size);
        break;
    case PROP_PROCESS_QUEUE_SIZE:
        g_value_set_int(value, self->process_queue_size);
        break;
    case PROP_POSTPROCESS_QUEUE_SIZE:
        g_value_set_int(value, self->postprocess_queue_size);
        break;
    case PROP_AGGREGATE_QUEUE_SIZE:
        g_value_set_int(value, self->aggregate_queue_size);
        break;
    case PROP_POSTAGGREGATE_QUEUE_SIZE:
        g_value_set_int(value, self->postaggregate_queue_size);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

G_DEFINE_TYPE_WITH_CODE(GstProcessBin, processbin, GST_TYPE_BIN,
                        GST_DEBUG_CATEGORY_INIT(processbin_debug, "processbin", 0, "debug category for processbin"));
