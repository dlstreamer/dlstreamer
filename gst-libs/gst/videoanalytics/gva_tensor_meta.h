/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __GVA_TENSOR_INFO_H__
#define __GVA_TENSOR_INFO_H__

#include <gst/gst.h>

#define GVA_TENSOR_META_TAG "gva_tensor_meta"

#define GVA_TENSOR_MAX_RANK 8

G_BEGIN_DECLS

/**
 * GVAPrecision:
 * @UNSPECIFIED: default value
 * @FP32: 32bit floating point value
 * @U8: unsignned 8bit integer value
 *
 * Enum value to be used with inference engine APIs to specify precision
 */
typedef enum {
    UNSPECIFIED = 255,
    FP32 = 10,
    U8 = 40,
} GVAPrecision;

/**
 * GVALayout:
 * @ANY: unspecified layout
 * @NCHW: NCWH layout
 * @NHWC: NHWC layout
 *
 * Enum value to be used with inference engine APIs to specify layer laouts
 */
typedef enum {
    ANY = 0,
    NCHW = 1,
    NHWC = 2,
} GVALayout;

typedef struct _GstGVATensorMeta GstGVATensorMeta;

/**
 * GstGVATensorMeta:
 * @meta: parent meta object
 * @precision: tensor precision see (#GVAPrecision)
 * @rank: tensor rank
 * @dims: array describing tensor's dimensions
 * @layout: tensor layout see (#GVALayout)
 * @layer_name: tensor output layer name
 * @model_name: model name
 * @data: tensor data
 * @total_bytes: tensor size in bytes
 * @element_id: TBD
 * @labels: array of strings can be used to assign text attributes to the #GVADetection.
 */
struct _GstGVATensorMeta {
    GstMeta meta;
    GVAPrecision precision;
    guint rank;
    size_t dims[GVA_TENSOR_MAX_RANK];
    GVALayout layout;
    gchar *layer_name;
    gchar *model_name;
    void *data;
    size_t total_bytes;
    const gchar *element_id;
};

/**
 * gst_gva_tensor_meta_get_info:
 * Returns: #GstMetaInfo for the registered meta type
 */
const GstMetaInfo *gst_gva_tensor_meta_get_info(void);

/**
 * gst_gva_tensor_meta_api_get_type:
 *
 * Returns: type for APIs binded to the #GstGVATensorMeta
 */
GType gst_gva_tensor_meta_api_get_type(void);

/**
 * gst_gva_tensor_meta_get_info:
 *
 * Register meta API if needed and
 *
 * Returns: #GstMetaInfo for registered type
 */
#define GST_GVA_TENSOR_META_INFO (gst_gva_tensor_meta_get_info())

/**
 * GST_GVA_TENSOR_META_GET:
 * @buf: #GstBuffer where meta will be created
 *
 * Adds #GstGVATensorMeta to the passed #GstBuffer
 *
 * Returns: pointer to the create Meta
 */
#define GST_GVA_TENSOR_META_GET(buf) ((GstGVATensorMeta *)gst_buffer_get_meta(buf, gst_gva_tensor_meta_api_get_type()))

/**
 * GST_GVA_TENSOR_META_ITERATE:
 * @buf: #GstBuffer to iterate through
 * @state: variable (of #gpointer type) containing loop state
 *
 * Helper macro to iterate through all detection metas of #GstBuffer
 * |[<!--language="C" -->
 *   gpointer state = NULL;
 *   while (meta = GST_GVA_TENSOR_META_ITERATE(buffer, &state)) {
 *   // ...
 *   }
 *
 * Returns: #GstGVATensorMeta
 */
#define GST_GVA_TENSOR_META_ITERATE(buf, state)                                                                        \
    ((GstGVATensorMeta *)gst_buffer_iterate_meta_filtered(buf, state, gst_gva_tensor_meta_api_get_type()))

/**
 * GST_GVA_TENSOR_META_ADD:
 * @buf: #GstBuffer where to create #GstGVATensorMeta
 *
 * Creates #GstGVATensorMeta and
 *
 * Returns: pointer to newly created meta
 */
#define GST_GVA_TENSOR_META_ADD(buf)                                                                                   \
    ((GstGVATensorMeta *)gst_buffer_add_meta(buf, gst_gva_tensor_meta_get_info(), NULL))

/**
 * GST_GVA_TENSOR_META_COUNT:
 * @buf: #GstBuffer to be processed
 *
 * Calculate a number of #GstGVATensorMeta in GstBuffer
 *
 * Returns: number of #GstGVATensorMeta metas
 */
#define GST_GVA_TENSOR_META_COUNT(buf) (gst_buffer_get_n_meta(buf, gst_gva_tensor_meta_api_get_type()))

/**
 * find_tensor_meta:
 * @buffer: #GstBuffer where it looks for the meta
 * @model_name: substring that should be in meta's model_name
 * @output_layer: substring that should be in meta's output_layer name
 *
 * Looks for the tensor meta conforming to passed parameters
 *
 * Returns: found meta or NULL otherwise
 */
GstGVATensorMeta *find_tensor_meta(GstBuffer *buffer, const char *model_name, const char *output_layer);

/**
 * find_tensor_meta_ext:
 * @buffer: #GstBuffer where it looks for the meta
 * @model_name: substring that should be in meta's model_name
 * @output_layer: substring that should be in meta's output_layer name
 * @element_id: element_id in meta
 * Looks for the tensor meta conforming to passed parameters
 *
 * Returns: found meta or NULL otherwise
 */
GstGVATensorMeta *find_tensor_meta_ext(GstBuffer *buffer, const char *model_name, const char *output_layer,
                                       const char *element_id);

/**
 * gva_tensor_number_elements:
 * @meta: #GstGVATensorMeta to get value
 *
 * Returns: number of elements in tensor
 */
guint gva_tensor_number_elements(GstGVATensorMeta *meta);

/**
 * gva_tensor_size:
 * @meta: #GstGVATensorMeta to get value
 *
 * Returns: tensor's size
 */
guint gva_tensor_size(GstGVATensorMeta *meta);

/**
 * gva_tensor_element_size:
 * @meta: #GstGVATensorMeta to get value
 *
 * Returns: Size of tensor's element
 */
guint gva_tensor_element_size(GstGVATensorMeta *meta);

/**
 * gva_tensor_get_element:
 * @meta: #GstGVATensorMeta to get value
 * @index: index  of element to get
 *
 * Returns: One element form the tensor by it's index
 */
void *gva_tensor_get_element(GstGVATensorMeta *meta, int index);

G_END_DECLS

/////////////////////////////////////////////////////////////////////////////////////////
// These are functions working with tensors produced by gvaclassify element,
// with tensor first dimension representing number detected objects.

#ifndef __GTK_DOC_IGNORE__
#ifdef __cplusplus
template <typename T>
T *GVATensorGetElement(GstGVATensorMeta *meta, int index) {
    int number_elements = gva_tensor_number_elements(meta);
    if (index < 0 || index > number_elements)
        return nullptr;
    if (meta->total_bytes != number_elements * sizeof(T))
        return nullptr;
    return (T *)((int8_t *)meta->data + index * meta->total_bytes / number_elements);
}
#endif

#endif /* __GTK_DOC_IGNORE__ */
#endif /* __GVA_TENSOR_INFO_H__ */
