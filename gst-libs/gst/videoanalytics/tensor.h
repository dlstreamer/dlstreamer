/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

/**
 * @file tensor.h
 * @brief This file contains GVA::Tensor class which contains and describes neural network inference result
 */

#ifndef __TENSOR_H__
#define __TENSOR_H__

#include <gst/gst.h>
#include <gst/video/gstvideometa.h>

#ifdef __cplusplus
extern "C" {
#endif // #ifdef __cplusplus

/**
 * @brief This enum describes model layer precision
 */
typedef enum {
    UNSPECIFIED = 255, /**< default value */
    FP32 = 10,         /**< 32bit floating point value */
    U8 = 40            /**< unsignned 8bit integer value */
} GVAPrecision;

/**
 * @brief This enum describes model layer layout
 */
typedef enum {
    ANY = 0,  /**< unspecified layout */
    NCHW = 1, /**< NCWH layout */
    NHWC = 2, /**< NHWC layout */
    NC = 193  /**< NC layout */
} GVALayout;

/**
 * @brief This function returns a pointer to the fixed array of tensor bytes
 * @param s GstStructure* to get tensor from. It's assumed that tensor data is stored in "data_buffer" field
 * @param[out] nbytes pointer to the location to store the number of bytes in returned array
 * @return void* to tensor data as bytes, NULL if s has no "data_buffer" field
 */
inline const void *gva_get_tensor_data(GstStructure *s, gsize *nbytes) {
    const GValue *f = gst_structure_get_value(s, "data_buffer");
    if (!f)
        return NULL;
    GVariant *v = g_value_get_variant(f);
    return g_variant_get_fixed_array(v, nbytes, 1);
}

#ifdef __cplusplus
}
#endif // #ifdef __cplusplus

#ifdef __cplusplus

#include <stdexcept>
#include <string>
#include <vector>

namespace GVA {
/**
 * @brief This class represents tensor - map-like storage for inference result information, such as output blob
 * description (output layer dims, layout, rank, precision, etc.), inference result in a raw and interpreted forms.
 * Tensor is based on GstStructure and, in general, can contain arbitrary (user-defined) fields of simplest data types,
 * like integers, floats & strings.
 * Tensor can contain only raw inference result (such Tensor is produced by gvainference in Gstreamer pipeline),
 * detection result (such Tensor is produced by gvadetect in Gstreamer pipeline and it's called detection Tensor), or
 * both raw & interpreted inference results (such Tensor is produced by gvaclassify in Gstreamer pipeline).
 * Tensors can be created and used on their own, or they can be created within RegionOfInterest or VideoFrame instances.
 * Usually, in Gstreamer pipeline with GVA elements (gvadetect, gvainference, gvaclassify) Tensor objects will be
 * available for access and modification from RegionOfInterest and VideoFrame instances
 */
class Tensor {
  public:
    /**
     * @brief Construct Tensor instance from GstStructure. Tensor does not own structure, so if you use this
     * consrtuctor, free structure after Tensor's lifetime, if needed
     * @param structure GstStructure to create Tensor instance from.
     */
    Tensor(GstStructure *structure) : _structure(structure) {
        if (not _structure)
            throw std::invalid_argument("GVA::Tensor: structure is nullptr");
    }

    /**
     * @brief Get vector of fields contained in Tensor instance
     * @return vector of fields contained in Tensor instance
     */
    std::vector<std::string> fields() const {
        std::vector<std::string> fields;
        int fields_count = gst_structure_n_fields(_structure);
        if (fields_count <= 0)
            return fields;

        fields.reserve(fields_count);
        for (int i = 0; i < fields_count; ++i)
            fields.emplace_back(gst_structure_nth_field_name(_structure, i));
        return fields;
    }

    /**
     * @brief Check if Tensor instance has field
     * @param field_name field name
     * @return True if field with this name is found, False otherwise
     */
    bool has_field(const std::string &field_name) const {
        return gst_structure_has_field(_structure, field_name.c_str());
    }

    /**
     * @brief Get string contained in value stored at field_name
     * @param field_name field name
     * @return string value stored at field_name if field_name is found and contains a string, empty string
     * otherwise
     */
    std::string get_string(const std::string &field_name) const {
        const gchar *val = gst_structure_get_string(_structure, field_name.c_str());
        return (val) ? std::string(val) : std::string();
    }

    /**
     * @brief Get int contained in value stored at field_name
     * @param field_name field name
     * @return int value stored at field_name if field_name is found and contains an int, 0 otherwise
     */
    int get_int(const std::string &field_name) const {
        gint val = 0;
        gst_structure_get_int(_structure, field_name.c_str(), &val);
        return val;
    }

    /**
     * @brief Get pointer to GValueArray contained in value stored at field_name
     * @param field_name field name
     * @return pointer to GValueArray with value stored at field_name if field_name is found and contains an
     * GValueArray, 0 otherwise
     */
    GValueArray *get_array(const std::string &field_name) const {
        GValueArray *value_array = NULL;
        gst_structure_get_array(_structure, field_name.c_str(), &value_array);
        return value_array;
    }

    /**
     * @brief Set field_name with GValueArray
     * @param field_name field name
     * @param value_array pointer to GValueArray to set
     */
    void set_array(const std::string &field_name, const GValueArray *value_array) {
        gst_structure_set_array(_structure, field_name.c_str(), value_array);
    }

    /**
     * @brief Get double contained in value stored at field_name
     * @param field_name field name
     * @return double value stored at field_name if field_name is found and contains an double, 0 otherwise
     */
    double get_double(const std::string &field_name) const {
        double val = 0;
        gst_structure_get_double(_structure, field_name.c_str(), &val);
        return val;
    }

    /**
     * @brief Set field_name with string value
     * @param field_name field name
     * @param value value to set
     */
    void set_string(const std::string &field_name, const std::string &value) {
        gst_structure_set(_structure, field_name.c_str(), G_TYPE_STRING, value.c_str(), NULL);
    }

    /**
     * @brief Set field_name with int value
     * @param field_name field name
     * @param value value to set
     */
    void set_int(const std::string &field_name, int value) {
        gst_structure_set(_structure, field_name.c_str(), G_TYPE_INT, value, NULL);
    }

    /**
     * @brief Set field_name with double value
     * @param field_name field name
     * @param value value to set
     */
    void set_double(const std::string &field_name, double value) {
        gst_structure_set(_structure, field_name.c_str(), G_TYPE_DOUBLE, value, NULL);
    }

    /**
     * @brief Get raw inference result blob data
     * @tparam T type to interpret blob data
     * @return vector of values of type T representing raw inference data, empty vector if data can't be read
     */
    template <class T>
    const std::vector<T> data() const {
        gsize size = 0;
        const void *data = gva_get_tensor_data(_structure, &size);
        if (!data || !size)
            return std::vector<T>();
        return std::vector<T>((T *)data, (T *)((char *)data + size));
    }

    /**
     * @brief Get name as a string
     * @return Tensor instance's name
     */
    std::string name() const {
        const gchar *name = gst_structure_get_name(_structure);
        if (name)
            return std::string(name);
        return std::string{};
    }

    /**
     * @brief Set Tensor instance's name
     */
    void set_name(const std::string &name) {
        gst_structure_set_name(_structure, name.c_str());
    }

    /**
     * @brief Set label. It will throw an exception if called for detection Tensor
     * @param label label name as a string
     */
    void set_label(const std::string &label) {
        if (!this->is_detection())
            set_string("label", label);
        else
            throw std::runtime_error("Detection GVA::Tensor can't have label.");
    }

    /**
     * @brief Get inference result blob dimensions info
     * @return vector of dimensions. Empty vector if dims are not set
     */
    std::vector<guint> dims() const {
        GValueArray *arr = get_array("dims");
        std::vector<guint> dims;
        if (arr)
            for (guint i = 0; i < arr->n_values; ++i)
                dims.push_back(g_value_get_uint(g_value_array_get_nth(arr, i)));

        return dims;
    }

    /**
     * @brief Get inference result blob precision
     * @return GVAPrecision, GVAPrecision::UNSPECIFIED if can't be read
     */
    GVAPrecision precision() const {
        if (has_field("precision"))
            return (GVAPrecision)get_int("precision");
        else
            return GVAPrecision::UNSPECIFIED;
    }

    /**
     * @brief Get inference result blob precision as a string
     * @return precision as a string, "ANY" if can't be read
     */
    std::string precision_as_string() const {
        GVAPrecision precision_value = precision();
        switch (precision_value) {
        case GVAPrecision::U8:
            return "U8";
        case GVAPrecision::FP32:
            return "FP32";
        default:
            return "UNSPECIFIED";
        }
    }

    /**
     * @brief Get inference result blob layout
     * @return GVALayout, GVALayout::ANY if can't be read
     */
    GVALayout layout() const {
        if (has_field("layout"))
            return (GVALayout)get_int("layout");
        else
            return GVALayout::ANY;
    }

    /**
     * @brief Get inference result blob layout as a string
     * @return layout as a string, "ANY" if can't be read
     */
    std::string layout_as_string() const {
        GVALayout layout_value = layout();
        switch (layout_value) {
        case GVALayout::NCHW:
            return "NCHW";
        case GVALayout::NHWC:
            return "NHWC";
        case GVALayout::NC:
            return "NC";
        default:
            return "ANY";
        }
    }

    /**
     * @brief Get inference result blob layer name
     * @return layer name as a string, empty string if failed to get
     */
    std::string layer_name() const {
        return get_string("layer_name");
    }

    /**
     * @brief Get model name which was used for inference
     * @return model name as a string, empty string if failed to get
     */
    std::string model_name() const {
        return get_string("model_name");
    }

    /**
     * @brief Get format
     * @return format as a string, empty string if failed to get
     */
    std::string format() const {
        return get_string("format");
    }

    /**
     * @brief Get number of inference result blob dimensions
     * @return number of inference result blob dimensions, 0 if failed to get
     */
    int rank() const {
        return get_int("rank");
    }

    /**
     * @brief Get inference-id property value of GVA element from which this Tensor came
     * @return inference-id property value of GVA element from which this Tensor came, empty string if failed to get
     */
    std::string element_id() const {
        return get_string("element_id");
    }

    /**
     * @brief Get inference result blob size in bytes
     * @return inference result blob size in bytes, 0 if failed to get
     */
    int total_bytes() const {
        return get_int("total_bytes");
    }

    /**
     * @brief Get confidence of inference result
     * @return confidence of inference result as a double, 0 if failed to get
     */
    double confidence() const {
        return get_double("confidence");
    }

    /**
     * @brief Get label. This label is set for Tensor instances produced by gvaclassify element. It will throw an
     * exception if called for detection Tensor. To get detection class label, use RegionOfInterest::label
     * @return label as a string, empty string if failed to get
     */
    std::string label() const {
        if (!this->is_detection())
            return get_string("label");
        else
            throw std::runtime_error("Detection GVA::Tensor can't have label.");
    }

    /**
     * @brief Get object id
     * @return object id as an int, 0 if failed to get
     */
    int object_id() const {
        return get_int("object_id");
    }

    /**
     * @brief Get label id
     * @return label id as an int, 0 if failed to get
     */

    int label_id() const {
        return get_int("label_id");
    }

    /**
     * @brief Get ptr to underlying GstStructure
     * @return ptr to underlying GstStructure
     */
    GstStructure *gst_structure() const {
        return _structure;
    }

    /**
     * @brief Check if this Tensor is detection Tensor (contains detection results)
     * @return True if tensor contains detection results, False otherwise
     */
    bool is_detection() const {
        return name() == "detection";
    }

  protected:
    /**
     * @brief ptr to GstStructure that contains all tensor (inference results) data & info.
     */
    GstStructure *_structure;
};

} // namespace GVA

#endif // __cplusplus
#endif // __TENSOR_H__