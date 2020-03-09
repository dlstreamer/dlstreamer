/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef _GST_GVA_PYTHON_H_
#define _GST_GVA_PYTHON_H_

#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_GVA_PYTHON (gst_gva_python_get_type())
#define GST_GVA_PYTHON(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_PYTHON, GstGvaPython))
#define GST_GVA_PYTHON_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_PYTHON, GstGvaPythonClass))
#define GST_IS_GVA_PYTHON(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_PYTHON))
#define GST_IS_GVA_PYTHON_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_PYTHON))

typedef struct _GstGvaPython GstGvaPython;
typedef struct _GstGvaPythonClass GstGvaPythonClass;

typedef struct _object PyObject;

struct _GstGvaPython {
    GstBaseTransform base_gvapython;
    gchar *module_name;
    gchar *class_name;
    gchar *function_name;
    gchar *arg_string;
    struct PythonCallback *python_callback;
};

struct _GstGvaPythonClass {
    GstBaseTransformClass base_gvapython_class;
};

GType gst_gva_python_get_type(void);

G_END_DECLS

#endif
