/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef __METAPUBLISH_FILE_H__
#define __METAPUBLISH_FILE_H__

#include "i_metapublish_method.h"
#include <glib-object.h>
#include <stdio.h>

G_BEGIN_DECLS

#define METAPUBLISH_TYPE_FILE metapublish_file_get_type()
G_DECLARE_FINAL_TYPE(MetapublishFile, metapublish_file, METAPUBLISH, FILE, GObject)
#define STDOUT "stdout"
#define JSON_RECORD_PREFIX ",\n"
#define JSON_LINES_RECORD_SUFFIX "\n"

gboolean initialize_file(MetapublishFile *mp_file, char *file_path, GstGVAMetaPublishFileFormat file_format);
gboolean write_message(MetapublishFile *mp_file, GstGVAMetaPublishFileFormat file_format, gchar *json_message);
void write_message_prefix(MetapublishFile *mp_file, GstGVAMetaPublishFileFormat file_format);
void write_message_suffix(MetapublishFile *mp_file, GstGVAMetaPublishFileFormat file_format);
gboolean finalize_file(MetapublishFile *mp_file, char *file_path, GstGVAMetaPublishFileFormat file_format);

G_END_DECLS

#endif /* __METAPUBLISH_FILE_H__ */