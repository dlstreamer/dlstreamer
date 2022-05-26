/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/gst/utils.h"
#include "dlstreamer/vaapi/context.h"

namespace dlstreamer {

class GSTVAAPIContext : public VAAPIContext {
  public:
    GSTVAAPIContext(GstPad *pad) : VAAPIContext(nullptr) {
        query_va_display(pad);
    }
    GSTVAAPIContext(GstBaseTransform *element) : VAAPIContext(nullptr) {
        query_va_display(element->sinkpad);
    }
    ~GSTVAAPIContext() {
        if (_display_obj) {
            gst_object_unref(_display_obj);
            _display_obj = nullptr;
        }
        if (_context) {
            gst_context_unref(_context);
            _context = nullptr;
        }
    }

  private:
    static constexpr auto GST_VAAPI_DISPLAY_CONTEXT_TYPE_NAME = "gst.vaapi.Display";
    static constexpr auto GST_VAAPI_DISPLAY_CONTEXT_FIELD_NAME = "gst.vaapi.Display.GObject";
    static constexpr auto GST_VAAPI_DISPLAY_NAME = "va-display";

    GstContext *_context = nullptr;
    GstObject *_display_obj = nullptr;

    void query_va_display(GstPad *pad) {
        _context = gst_query_context(pad, GST_VAAPI_DISPLAY_CONTEXT_TYPE_NAME);

        const GstStructure *structure = gst_context_get_structure(_context);
        auto ret =
            gst_structure_get(structure, GST_VAAPI_DISPLAY_CONTEXT_FIELD_NAME, GST_TYPE_OBJECT, &_display_obj, NULL);
        if (!ret || !_display_obj)
            throw std::runtime_error("Couldn't get field from GST context");

        g_object_get(_display_obj, GST_VAAPI_DISPLAY_NAME, &_va_display, nullptr);
        if (!is_valid())
            throw std::runtime_error("Got invalid VADisplay from context");

        GST_INFO_OBJECT(pad, "Got VADisplay from VAAPI context: %p", _va_display);
    }
};

} // namespace dlstreamer
