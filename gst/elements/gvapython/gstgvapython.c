/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <Python.h>
#include <gst/gst.h>

#include "gstgvapython.h"
#include "gva_caps.h"
#include "python_callback_c.h"

#define UNUSED(x) (void)(x)

#define ELEMENT_LONG_NAME "Calls Python function on each frame and passes gi.repository.Gst.Buffer as parameter"
#define ELEMENT_DESCRIPTION ELEMENT_LONG_NAME

GST_DEBUG_CATEGORY_STATIC(gst_gva_python_debug_category);
#define GST_CAT_DEFAULT gst_gva_python_debug_category

enum { PROP_0, PROP_MODULE, PROP_CLASS, PROP_FUNCTION, PROP_ARGUMENT };

#define DEFAULT_MODULE ""
#define DEFAULT_CLASS ""
#define DEFAULT_FUNCTION "process_frame"
#define DEFAULT_ARGUMENT ""

#ifdef NDEBUG
#define LOG_PYTHON_ERROR(ELEMENT, ...) GST_ERROR_OBJECT(ELEMENT, __VA_ARGS__)
#else
#define LOG_PYTHON_ERROR(ELEMENT, ...)                                                                                 \
    GST_ERROR_OBJECT(ELEMENT, __VA_ARGS__);                                                                            \
    PyErr_Print()
#endif

/* prototypes */
static void gst_gva_python_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void gst_gva_python_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static gboolean gst_gva_python_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);
static void gst_gva_python_dispose(GObject *object);
static void gst_gva_python_finalize(GObject *object);

static GstFlowReturn gst_gva_python_transform_ip(GstBaseTransform *trans, GstBuffer *buf);

/* class initialization */
static void gst_gva_python_init(GstGvaPython *gva_python);

G_DEFINE_TYPE_WITH_CODE(GstGvaPython, gst_gva_python, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_python_debug_category, "gvapython", 0,
                                                "debug category for gvapython element"));

static void gst_gva_python_class_init(GstGvaPythonClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string("ANY")));
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_from_string("ANY")));

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");
    gobject_class->set_property = gst_gva_python_set_property;
    gobject_class->get_property = gst_gva_python_get_property;
    gobject_class->dispose = gst_gva_python_dispose;
    gobject_class->finalize = gst_gva_python_finalize;
    base_transform_class->set_caps = GST_DEBUG_FUNCPTR(gst_gva_python_set_caps);
    base_transform_class->transform = NULL;
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_gva_python_transform_ip);

    g_object_class_install_property(gobject_class, PROP_MODULE,
                                    g_param_spec_string("module", "Python module name", "Python module name",
                                                        DEFAULT_MODULE, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_CLASS,
                                    g_param_spec_string("class", "(optional) Python class name", "Python class name",
                                                        DEFAULT_CLASS, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_ARGUMENT,
                                    g_param_spec_string("arg",
                                                        "(optional) String argument for Python class initialization",
                                                        "String argument for Python class initialization",
                                                        DEFAULT_ARGUMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_FUNCTION,
                                    g_param_spec_string("function", "Python function name", "Python function name",
                                                        DEFAULT_FUNCTION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void gst_gva_python_init(GstGvaPython *gvapython) {
    GST_DEBUG_OBJECT(gvapython, "gva_python_init");
    gvapython->module_name = NULL;
    gvapython->class_name = NULL;
    gvapython->arg_string = NULL;
    gvapython->function_name = g_strdup(DEFAULT_FUNCTION);
    gvapython->python_callback = NULL;
}

void gst_gva_python_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    GstGvaPython *gvapython = GST_GVA_PYTHON(object);

    GST_DEBUG_OBJECT(gvapython, "get_property");
    switch (property_id) {
    case PROP_MODULE:
        g_value_set_string(value, gvapython->module_name);
        break;
    case PROP_CLASS:
        g_value_set_string(value, gvapython->class_name);
        break;
    case PROP_FUNCTION:
        g_value_set_string(value, gvapython->function_name);
        break;
    case PROP_ARGUMENT:
        g_value_set_string(value, gvapython->arg_string);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_python_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    GstGvaPython *gvapython = GST_GVA_PYTHON(object);
    GST_DEBUG_OBJECT(gvapython, "set_property");

    switch (property_id) {
    case PROP_MODULE:
        g_free(gvapython->module_name);
        gvapython->module_name = g_strdup(g_value_get_string(value));
        break;
    case PROP_CLASS:
        g_free(gvapython->class_name);
        gvapython->class_name = g_strdup(g_value_get_string(value));
        break;
    case PROP_FUNCTION:
        g_free(gvapython->function_name);
        gvapython->function_name = g_strdup(g_value_get_string(value));
        break;
    case PROP_ARGUMENT:
        g_free(gvapython->arg_string);
        gvapython->arg_string = g_strdup(g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static gboolean gst_gva_python_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps) {
    UNUSED(outcaps);
    GstGvaPython *gvapython = GST_GVA_PYTHON(trans);
    GST_DEBUG_OBJECT(gvapython, "set_caps");

    if (!gvapython->python_callback) {
        if (!gvapython->module_name) {
            GST_ERROR_OBJECT(gvapython, "Parameter 'module' not set");
            return FALSE;
        }

        gvapython->python_callback = create_python_callback(gvapython->module_name, gvapython->class_name,
                                                            gvapython->function_name, gvapython->arg_string, incaps);
    }

    return gvapython->python_callback != NULL;
}

void gst_gva_python_dispose(GObject *object) {
    GstGvaPython *gvapython = GST_GVA_PYTHON(object);

    GST_DEBUG_OBJECT(gvapython, "dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gst_gva_python_parent_class)->dispose(object);
}

void gst_gva_python_finalize(GObject *object) {
    GstGvaPython *gvapython = GST_GVA_PYTHON(object);

    GST_DEBUG_OBJECT(gvapython, "finalize");

    g_free(gvapython->module_name);
    gvapython->module_name = NULL;
    g_free(gvapython->class_name);
    gvapython->class_name = NULL;
    g_free(gvapython->function_name);
    gvapython->function_name = NULL;

    if (gvapython->python_callback) {
        delete_python_callback(gvapython->python_callback);
        gvapython->python_callback = NULL;
    }

    G_OBJECT_CLASS(gst_gva_python_parent_class)->finalize(object);
}

static GstFlowReturn gst_gva_python_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
    GstGvaPython *gvapython = GST_GVA_PYTHON(trans);
    GST_DEBUG_OBJECT(gvapython, "transform_ip");

    return invoke_python_callback(gvapython->python_callback, buf) ? GST_FLOW_OK : GST_FLOW_ERROR;
}

static gboolean plugin_init(GstPlugin *plugin) {
    if (!gst_element_register(plugin, "gvapython", GST_RANK_NONE, GST_TYPE_GVA_PYTHON)) {
        return FALSE;
    }
    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, gvapython, ELEMENT_DESCRIPTION, plugin_init, PLUGIN_VERSION,
                  PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
