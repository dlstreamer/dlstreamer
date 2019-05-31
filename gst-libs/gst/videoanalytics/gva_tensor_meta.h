/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

/**
 * @file gva_tensor_meta.h
 * @brief This file contains helper functions to control _GstGVATensorMeta instances
 */

#ifndef __GVA_TENSOR_META_H__
#define __GVA_TENSOR_META_H__

#include <gst/gst.h>

#define GVA_TENSOR_META_TAG "gva_tensor_meta"

#define GVA_TENSOR_MAX_RANK 8

G_BEGIN_DECLS

/**
 * @brief This enum describes model layer precision
 */
typedef enum {
    UNSPECIFIED = 255, /**< default value */
    FP32 = 10,         /**< 32bit floating point value */
    U8 = 40,           /**< unsigned 8bit integer value */
} GVAPrecision;

/**
 * @brief Enum value to be used with inference engine APIs to specify layer layouts
 */
typedef enum {
    ANY = 0,  /**< unspecified layout */
    NCHW = 1, /**< NCWH layout */
    NHWC = 2, /**< NHWC layout */
} GVALayout;

typedef struct _GstGVATensorMeta GstGVATensorMeta;

/**
 * @brief This struct represents raw tensor metadata and contains instance of parent GstMeta and fields describing
 * inference result tensor
 */
struct _GstGVATensorMeta {
    GstMeta meta;                     /**< parent meta object */
    GVAPrecision precision;           /**< tensor precision (see GVAPrecision) */
    guint rank;                       /**< tensor rank */
    size_t dims[GVA_TENSOR_MAX_RANK]; /**< array describing tensor's dimensions */
    GVALayout layout;                 /**< tensor layout (see GVALayout) */
    gchar *layer_name;                /**< tensor output layer name */
    gchar *model_name;                /**< model name */
    void *data;                       /**< tensor data */
    size_t total_bytes;               /**< tensor size in bytes */
    const gchar *element_id;          /**< id of GStreamer pipeline element that produced current tensor */
};

/**
 * @brief This function registers, if needed, and returns GstMetaInfo for _GstGVATensorMeta
 * @return GstMetaInfo* for registered type
 */
const GstMetaInfo *gst_gva_tensor_meta_get_info(void);

/**
 * @brief This function registers, if needed, and returns a GType for api "GstGVATensorMetaAPI" and associate it with
 * GVA_TENSOR_META_TAG tag
 * @return GType type
 */
GType gst_gva_tensor_meta_api_get_type(void);

/**
 * @def GST_GVA_TENSOR_META_INFO
 * @brief This macro calls gst_gva_tensor_meta_get_info
 * @return const GstMetaInfo* for registered type
 */
#define GST_GVA_TENSOR_META_INFO (gst_gva_tensor_meta_get_info())

/**
 * @def GST_GVA_TENSOR_META_GET
 * @brief This macro retrieves ptr to _GstGVATensorMeta instance for passed buf
 * @param buf GstBuffer* of which metadata is retrieved
 * @return _GstGVATensorMeta* instance attached to buf
 */
#define GST_GVA_TENSOR_META_GET(buf) ((GstGVATensorMeta *)gst_buffer_get_meta(buf, gst_gva_tensor_meta_api_get_type()))

/**
 * @def GST_GVA_TENSOR_META_ITERATE
 * @brief This macro iterates through _GstGVATensorMeta instances for passed buf, retrieving the next _GstGVATensorMeta.
 * If state points to NULL, the first _GstGVATensorMeta is returned
 * @param buf GstBuffer* of which metadata is iterated and retrieved
 * @param state gpointer* that updates with opaque pointer after macro call.
 * @return _GstGVATensorMeta* instance attached to buf
 */
#define GST_GVA_TENSOR_META_ITERATE(buf, state)                                                                        \
    ((GstGVATensorMeta *)gst_buffer_iterate_meta_filtered(buf, state, gst_gva_tensor_meta_api_get_type()))

/**
 * @def GST_GVA_TENSOR_META_ADD
 * @brief This macro attaches new _GstGVATensorMeta instance to passed buf
 * @param buf GstBuffer* to which metadata will be attached
 * @return _GstGVATensorMeta* of the newly added instance attached to buf
 */
#define GST_GVA_TENSOR_META_ADD(buf)                                                                                   \
    ((GstGVATensorMeta *)gst_buffer_add_meta(buf, gst_gva_tensor_meta_get_info(), NULL))

/**
 * @def GST_GVA_TENSOR_META_COUNT
 * @brief This macro counts the number of _GstGVATensorMeta instances attached to passed buf
 * @param buf GstBuffer* of which metadata instances are counted
 * @return guint number of _GstGVATensorMeta instances attached to passed buf
 */
#define GST_GVA_TENSOR_META_COUNT(buf) (gst_buffer_get_n_meta(buf, gst_gva_tensor_meta_api_get_type()))

/**
 * @brief This function searches for first _GstGVATensorMeta instance that satisfies passed parameters
 * @param buffer GstBuffer* that is searched for metadata
 * @param model_name substring that should be in _GstGVATensorMeta instance's model_name
 * @param output_layer substring that should be in _GstGVATensorMeta instance's layer_name
 * @return GstGVATensorMeta* for found instance or NULL if none are found
 */
GstGVATensorMeta *find_tensor_meta(GstBuffer *buffer, const char *model_name, const char *output_layer);

/**
 * @brief This function searches for first _GstGVATensorMeta instance that satisfies passed parameters
 * @param buffer GstBuffer* that is searched for metadata
 * @param model_name substring that should be in _GstGVATensorMeta instance's model_name
 * @param output_layer substring that should be in _GstGVATensorMeta instance's layer_name
 * @param element_id element_id substring that should be in _GstGVATensorMeta instance's element_id
 * @return GstGVATensorMeta* for found instance or NULL if none are found
 */
GstGVATensorMeta *find_tensor_meta_ext(GstBuffer *buffer, const char *model_name, const char *output_layer,
                                       const char *element_id);

/**
 * @brief This function returns number of elements in tensor as product of its dimensions
 * @param meta _GstGVATensorMeta to get value from
 * @return number of elements
 */
guint gva_tensor_size(GstGVATensorMeta *meta);

G_END_DECLS

#endif /* __GVA_TENSOR_META_H__ */
