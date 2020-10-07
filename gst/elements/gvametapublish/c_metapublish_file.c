/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "c_metapublish_file.h"
GST_DEBUG_CATEGORY_STATIC(gst_gva_meta_publish_debug_category);
#define GST_CAT_DEFAULT gst_gva_meta_publish_debug_category

struct _MetapublishFile {
    GObject parent_instance;
    FILE *output_file;
};

static void metapublish_file_method_interface_init(MetapublishMethodInterface *iface);

G_DEFINE_TYPE_WITH_CODE(MetapublishFile, metapublish_file, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(METAPUBLISH_TYPE_METHOD, metapublish_file_method_interface_init))

static gboolean metapublish_file_method_start(MetapublishMethod *self, GstGvaMetaPublish *gvametapublish) {
    GST_DEBUG_CATEGORY_INIT(gst_gva_meta_publish_debug_category, "gvametapublish", 0,
                            "debug category for gvametapublish element");
    MetapublishFile *mp_file = (MetapublishFile *)self;
    if (!gvametapublish->file_path) {
        GST_ELEMENT_ERROR(gvametapublish, RESOURCE, NOT_FOUND, ("file_path cannot be NULL."), (NULL));
        return FALSE;
    }
    if (!initialize_file(mp_file, gvametapublish->file_path, gvametapublish->file_format)) {
        GST_ELEMENT_ERROR(gvametapublish, RESOURCE, NOT_FOUND, ("Error opening file %s.", gvametapublish->file_path),
                          (NULL));
        return FALSE;
    }
    return TRUE;
}

static gboolean metapublish_file_method_publish(MetapublishMethod *self, GstGvaMetaPublish *gvametapublish,
                                                gchar *json_message) {
    MetapublishFile *mp_file = (MetapublishFile *)self;
    if (!json_message) {
        GST_DEBUG_OBJECT(gvametapublish, "No JSON message.");
        return TRUE;
    }
    if (!write_message(mp_file, gvametapublish->file_format, json_message)) {
        GST_ERROR_OBJECT(gvametapublish, "Error writing inference to file.");
        return FALSE;
    }
    GST_DEBUG_OBJECT(gvametapublish, "Message written successfully.");
    return TRUE;
}

static gboolean metapublish_file_method_stop(MetapublishMethod *self, GstGvaMetaPublish *gvametapublish) {
    MetapublishFile *mp_file = (MetapublishFile *)self;
    if (!finalize_file(mp_file, gvametapublish->file_path, gvametapublish->file_format)) {
        GST_ERROR_OBJECT(gvametapublish, "Error finalizing file.");
        return FALSE;
    }
    GST_DEBUG_OBJECT(gvametapublish, "File finalized successfully.");
    return TRUE;
}

static void metapublish_file_method_interface_init(MetapublishMethodInterface *iface) {
    iface->start = metapublish_file_method_start;
    iface->publish = metapublish_file_method_publish;
    iface->stop = metapublish_file_method_stop;
}

static void metapublish_file_init(MetapublishFile *self) {
    (void)self;
}

static void metapublish_file_class_init(MetapublishFileClass *klass) {
    (void)klass;
}

gboolean initialize_file(MetapublishFile *mp_file, char *file_path, GstGVAMetaPublishFileFormat file_format) {
    if (strcmp(file_path, STDOUT) == 0) {
        mp_file->output_file = stdout;
        return TRUE;
    }
    if (file_format == GST_GVA_METAPUBLISH_JSON) {
        if (!(mp_file->output_file = fopen(file_path, "w+"))) {
            return FALSE;
        }
        // File will be an array of JSON objects. Start the array with '['
        fputs("[", mp_file->output_file);
    } else { // GST_GVA_METAPUBLISH_JSON_LINES
        if (!(mp_file->output_file = fopen(file_path, "a+"))) {
            return FALSE;
        }
    }
    return TRUE;
}

gboolean write_message(MetapublishFile *mp_file, GstGVAMetaPublishFileFormat file_format, gchar *json_message) {
    if (!mp_file->output_file)
        return FALSE;
    write_message_prefix(mp_file, file_format);
    fputs(json_message, mp_file->output_file);
    write_message_suffix(mp_file, file_format);
    fflush(mp_file->output_file);
    return TRUE;
}

void write_message_prefix(MetapublishFile *mp_file, GstGVAMetaPublishFileFormat file_format) {
    // Add comma and line feed before the record when producing a JSON
    if (file_format == GST_GVA_METAPUBLISH_JSON) {
        if (ftello(mp_file->output_file) > 2) {
            // a prior record was written, precede this message with record separator
            fputs(JSON_RECORD_PREFIX, mp_file->output_file);
        }
    }
}

void write_message_suffix(MetapublishFile *mp_file, GstGVAMetaPublishFileFormat file_format) {
    // Add line feed after each record when producing a JSON Lines file/FIFO
    if (file_format == GST_GVA_METAPUBLISH_JSON_LINES) {
        fputs(JSON_LINES_RECORD_SUFFIX, mp_file->output_file);
    }
}

gboolean finalize_file(MetapublishFile *mp_file, char *file_path, GstGVAMetaPublishFileFormat file_format) {
    if (!mp_file->output_file) {
        return FALSE;
    }
    if (file_format == GST_GVA_METAPUBLISH_JSON && ftello(mp_file->output_file) > 0) {
        fputs("]", mp_file->output_file);
    }
    fputs("\n", mp_file->output_file);
    // For any pathfile we initialized w/ fopen(), invoke corresponding fclose()
    if (strcmp(file_path, STDOUT) != 0) {
        if (fclose(mp_file->output_file) != 0) {
            return FALSE;
        }
    }
    return TRUE;
}
