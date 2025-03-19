/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "common.hpp"

GstStaticPadTemplate gva_meta_publish_sink_template =
    GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS("ANY"));
GstStaticPadTemplate gva_meta_publish_src_template =
    GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS("ANY"));

const gchar *file_format_to_string(FileFormat format) {
    switch (format) {
    case GVA_META_PUBLISH_JSON:
        return FILE_FORMAT_JSON_NAME;
    case GVA_META_PUBLISH_JSON_LINES:
        return FILE_FORMAT_JSON_LINES_NAME;
    default:
        return UNKNOWN_VALUE_NAME;
    }
}

GType gva_metapublish_file_format_get_type(void) {
    static GType gva_metapublish_file_format_type = 0;
    static const GEnumValue file_format_types[] = {
        {GVA_META_PUBLISH_JSON, "the whole file is valid JSON array where each element is inference results per frame ",
         FILE_FORMAT_JSON_NAME},
        {GVA_META_PUBLISH_JSON_LINES, "each line is valid JSON with inference results per frame",
         FILE_FORMAT_JSON_LINES_NAME},
        {0, nullptr, nullptr}};

    if (!gva_metapublish_file_format_type) {
        gva_metapublish_file_format_type = g_enum_register_static("GvaMetaPublishFileFormat", file_format_types);
    }

    return gva_metapublish_file_format_type;
}
