/*******************************************************************************
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

/**
 * @file tensor.h
 * @brief This file contains GVA::Tensor class which contains and describes neural network inference result
 */

#ifndef __TENSOR_H__
#define __TENSOR_H__

#include "../metadata/gva_tensor_meta.h"

#include <gst/gst.h>
#include <gst/video/gstvideometa.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace GVA {

/**
 * @brief This class represents tensor - map-like storage for inference result information, such as output blob
 * description (output layer dims, layout, rank, precision, etc.), inference result in a raw and interpreted forms.
 * Tensor is based on GstStructure and, in general, can contain arbitrary (user-defined) fields of simplest data types,
 * like integers, floats & strings.
 * Tensor can contain raw inference result (such Tensor is produced by gvainference in Gstreamer pipeline),
 * detection result (such Tensor is produced by gvadetect in Gstreamer pipeline and it's called detection Tensor),
 * or both raw & interpreted inference results (such Tensor is produced by gvaclassify in Gstreamer pipeline).
 * Tensors can be created and used on their own, or they can be created within RegionOfInterest or VideoFrame instances.
 * Usually, in Gstreamer pipeline with GVA elements (gvadetect, gvainference, gvaclassify) Tensor objects will be
 * available for access and modification from RegionOfInterest and VideoFrame instances
 */
class Tensor {
    friend class VideoFrame;
#ifdef AUDIO
    friend class AudioFrame;
#endif

  public:
    /**
     * @brief Describes tensor precision
     */
    enum class Precision {
        UNSPECIFIED = GVA_PRECISION_UNSPECIFIED, /**< default value */
        FP32 = GVA_PRECISION_FP32,               /**< 32bit floating point value */
        FP16 = GVA_PRECISION_FP16,    /**< 16bit floating point value, 5 bit for exponent, 10 bit for mantisa */
        BF16 = GVA_PRECISION_BF16,    /**< 16bit floating point value, 8 bit for exponent, 7 bit for mantisa*/
        FP64 = GVA_PRECISION_FP64,    /**< 64bit floating point value */
        Q78 = GVA_PRECISION_Q78,      /**< 16bit specific signed fixed point precision */
        I16 = GVA_PRECISION_I16,      /**< 16bit signed integer value */
        U4 = GVA_PRECISION_U4,        /**< 4bit unsigned integer value */
        U8 = GVA_PRECISION_U8,        /**< unsigned 8bit integer value */
        I4 = GVA_PRECISION_I4,        /**< 4bit signed integer value */
        I8 = GVA_PRECISION_I8,        /**< 8bit signed integer value */
        U16 = GVA_PRECISION_U16,      /**< 16bit unsigned integer value */
        I32 = GVA_PRECISION_I32,      /**< 32bit signed integer value */
        U32 = GVA_PRECISION_U32,      /**< 32bit unsigned integer value */
        I64 = GVA_PRECISION_I64,      /**< 64bit signed integer value */
        U64 = GVA_PRECISION_U64,      /**< 64bit unsigned integer value */
        BIN = GVA_PRECISION_BIN,      /**< 1bit integer value */
        BOOL = GVA_PRECISION_BOOL,    /**< 8bit bool type */
        CUSTOM = GVA_PRECISION_CUSTOM /**< custom precision has it's own name and size of elements */
    };

    /**
     * @brief Describes tensor layout
     */
    enum class Layout {
        ANY = GVA_LAYOUT_ANY,   /**< unspecified layout */
        NCHW = GVA_LAYOUT_NCHW, /**< NCWH layout */
        NHWC = GVA_LAYOUT_NHWC, /**< NHWC layout */
        NC = GVA_LAYOUT_NC      /**< NC layout */
    };

    /**
     * @brief Get raw inference output blob data
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
     * @brief Get inference result blob dimensions info
     * @return vector of dimensions. Empty vector if dims are not set
     */
    std::vector<guint> dims() const {
        GValueArray *arr = NULL;
        gst_structure_get_array(_structure, "dims", &arr);
        std::vector<guint> dims;
        if (arr) {
            for (guint i = 0; i < arr->n_values; ++i)
                dims.push_back(g_value_get_uint(g_value_array_get_nth(arr, i)));
            g_value_array_free(arr);
        }
        return dims;
    }

    /**
     * @brief Get inference output blob precision
     * @return Enum Precision, Precision::UNSPECIFIED if can't be read
     */
    Precision precision() const {
        if (has_field("precision"))
            return (Precision)get_int("precision");
        else
            return Precision::UNSPECIFIED;
    }

    /**
     * @brief Get inference result blob layout
     * @return Enum Layout, Layout::ANY if can't be read
     */
    Layout layout() const {
        if (has_field("layout"))
            return (Layout)get_int("layout");
        else
            return Layout::ANY;
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
     * @brief Get data format as specified in model pre/post-processing json configuration
     * @return format as a string, empty string if failed to get
     */
    std::string format() const {
        return get_string("format");
    }

    /**
     * @brief Get tensor name as a string
     * @return Tensor instance's name
     */
    std::string name() const {
        const gchar *name = gst_structure_get_name(_structure);
        if (name)
            return std::string(name);
        return std::string{};
    }

    /**
     * @brief Get confidence of detection or classification result extracted from the tensor
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
     * @param default_value default value
     * @return string value stored at field_name if field_name is found and contains a string, default_value string
     * otherwise
     */
    std::string get_string(const std::string &field_name, const std::string &default_value = std::string()) const {
        const gchar *val = gst_structure_get_string(_structure, field_name.c_str());
        return (val) ? std::string(val) : default_value;
    }

    /**
     * @brief Get int contained in value stored at field_name
     * @param field_name field name
     * @param default_value default value
     * @return int value stored at field_name if field_name is found and contains an int, default_value otherwise
     */
    int get_int(const std::string &field_name, int32_t default_value = 0) const {
        gint val = default_value;
        gst_structure_get_int(_structure, field_name.c_str(), &val);
        return val;
    }

    /**
     * @brief Get double contained in value stored at field_name
     * @param field_name field name
     * @param default_value default value
     * @return double value stored at field_name if field_name is found and contains an double, default_value otherwise
     */
    double get_double(const std::string &field_name, double default_value = 0) const {
        double val = default_value;
        gst_structure_get_double(_structure, field_name.c_str(), &val);
        return val;
    }

    /**
     * @brief Get float vector contained in value stored at field_name
     * @param field_name field name
     * @return float vector stored at field_name if field_name is found and contains an float array, empty vector
     * otherwise
     */
    std::vector<float> get_float_vector(const std::string &field_name) const {
        GValueArray *arr = NULL;
        gst_structure_get_array(_structure, field_name.c_str(), &arr);
        std::vector<float> result;
        if (arr) {
            for (guint i = 0; i < arr->n_values; ++i)
                result.push_back(g_value_get_float(g_value_array_get_nth(arr, i)));
            g_value_array_free(arr);
        }
        return result;
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
     * @brief Get inference result blob precision as a string
     * @return precision as a string, "ANY" if can't be read
     */
    std::string precision_as_string() const {
        Precision precision_value = precision();
        switch (precision_value) {
        case Precision::FP32:
            return "FP32";
        case Precision::FP16:
            return "FP16";
        case Precision::BF16:
            return "BF16";
        case Precision::FP64:
            return "FP64";
        case Precision::Q78:
            return "Q78";
        case Precision::I16:
            return "I16";
        case Precision::U4:
            return "U4";
        case Precision::U8:
            return "U8";
        case Precision::I4:
            return "I4";
        case Precision::I8:
            return "I8";
        case Precision::U16:
            return "U16";
        case Precision::I32:
            return "I32";
        case Precision::U32:
            return "U32";
        case Precision::I64:
            return "I64";
        case Precision::U64:
            return "U64";
        case Precision::BIN:
            return "BIN";
        case Precision::BOOL:
            return "BOOL";
        case Precision::CUSTOM:
            return "CUSTOM";
        default:
            return "UNSPECIFIED";
        }
    }

    /**
     * @brief Get inference result blob layout as a string
     * @return layout as a string, "ANY" if can't be read
     */
    std::string layout_as_string() const {
        Layout layout_value = layout();
        switch (layout_value) {
        case Layout::NCHW:
            return "NCHW";
        case Layout::NHWC:
            return "NHWC";
        case Layout::NC:
            return "NC";
        default:
            return "ANY";
        }
    }

    /**
     * @brief Get inference-id property value of GVA element from which this Tensor came
     * @return inference-id property value of GVA element from which this Tensor came, empty string if failed to get
     */
    std::string element_id() const {
        return get_string("element_id");
    }

    /**
     * @brief Get label id
     * @return label id as an int, 0 if failed to get
     */

    int label_id() const {
        return get_int("label_id");
    }

    /**
     * @brief Check if this Tensor is detection Tensor (contains detection results)
     * @return True if tensor contains detection results, False otherwise
     */
    bool is_detection() const {
        return name() == "detection";
    }

    /**
     * @brief Construct Tensor instance from GstStructure. Tensor does not own structure, so if you use this
     * constructor, free structure after Tensor's lifetime, if needed
     * @param structure GstStructure to create Tensor instance from.
     */
    Tensor(GstStructure *structure) : _structure(structure) {
        if (not _structure)
            throw std::invalid_argument("GVA::Tensor: structure is nullptr");
    }

    /**
     * @brief Get ptr to underlying GstStructure
     * @return ptr to underlying GstStructure
     */
    GstStructure *gst_structure() const {
        return _structure;
    }

  protected:
    /**
     * @brief ptr to GstStructure that contains all tensor (inference results) data & info.
     */
    GstStructure *_structure;
};

} // namespace GVA

#endif // __TENSOR_H__
