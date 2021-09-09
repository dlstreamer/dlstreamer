/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"

#include "gst_vaapi_helper.h"

#include "scope_guard.h"

#ifdef ENABLE_VAAPI
#include "vaapi_utils.h"
#endif

namespace VaapiHelper {

#ifdef ENABLE_VAAPI

VaApiDisplayPtr queryDisplayInternal(GstPad *pad, const gchar *context_type, const gchar *field_name) {
    GstQuery *query = gst_query_new_context(context_type);
    auto query_sg = makeScopeGuard([query] { gst_query_unref(query); });

    gboolean ret = gst_pad_peer_query(pad, query);
    if (!ret) {
        GST_WARNING_OBJECT(pad, "Couldn't query GST-VA context by type '%s'", context_type);
        return {};
    }

    GstContext *context = nullptr;
    gst_query_parse_context(query, &context);

    GST_INFO_OBJECT(pad, "Got GST-VA context: %" GST_PTR_FORMAT, context);

    const GstStructure *structure = gst_context_get_structure(context);

    GstObject *display_obj = nullptr;
    ret = gst_structure_get(structure, field_name, GST_TYPE_OBJECT, &display_obj, NULL);
    if (!ret || !display_obj) {
        GST_ERROR_OBJECT(pad, "Couldn't parse context!");
        return {};
    }

    VADisplay dpy_ptr = nullptr;
    g_object_get(display_obj, "va-display", &dpy_ptr, nullptr);

    if (!VaDpyWrapper::isDisplayValid(dpy_ptr)) {
        GST_ERROR_OBJECT(pad, "Got invalid VADisplay from context!");
        gst_object_unref(display_obj);
        return {};
    }

    GST_INFO_OBJECT(pad, "Got VADisplay from context: %p", dpy_ptr);

    auto deleter = [display_obj](void *) { gst_object_unref(display_obj); };

    return VaApiDisplayPtr(dpy_ptr, deleter);
}

VaApiDisplayPtr queryVaDisplay(GstBaseTransform *element) {
    static const char GST_VAAPI_DISPLAY_CONTEXT_TYPE_NAME[] = "gst.vaapi.Display";

    return queryDisplayInternal(element->sinkpad, GST_VAAPI_DISPLAY_CONTEXT_TYPE_NAME,
                                GST_VAAPI_DISPLAY_CONTEXT_TYPE_NAME);
}

#else

VaApiDisplayPtr queryVaDisplay(GstBaseTransform *element) {
    GST_WARNING_OBJECT(element, "Couldn't query VaDisplay: project was built without VAAPI support.");
    return {};
}

#endif

} // namespace VaapiHelper
