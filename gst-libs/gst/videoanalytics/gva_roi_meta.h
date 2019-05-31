/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

/**
 * @file gva_roi_meta.h
 * @brief This file contains classes & functions to control GVA::RegionOfInterest and GVA::Tensor instances
 */

#pragma once

#include <gst/gst.h>
#include <gst/video/gstvideometa.h>

/**
 * @def GST_VIDEO_REGION_OF_INTEREST_META_ITERATE
 * @brief This macro iterates through GstVideoRegionOfInterestMeta instances for passed buf, retrieving the next
 * GstVideoRegionOfInterestMeta. If state points to NULL, the first GstVideoRegionOfInterestMeta is returned
 * @param buf GstBuffer* of which metadata is iterated and retrieved
 * @param state gpointer* that updates with opaque pointer after macro call.
 * @return GstVideoRegionOfInterestMeta* instance attached to buf
 */
#define GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buf, state)                                                          \
    ((GstVideoRegionOfInterestMeta *)gst_buffer_iterate_meta_filtered(buf, state,                                      \
                                                                      GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE))

#ifdef __cplusplus
extern "C" {
#endif // #ifdef __cplusplus
/**
 * @brief This function returns a pointer to the fixed array of tensor bytes
 * @param s GstStructure* to get tensor from. It's assumed that tensor data is stored in "data_buffer" field
 * @param[out] nbytes pointer to the location to store the number of bytes in returned array
 * @return void* to tensor data as bytes, NULL if s has not "data_buffer" field
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

#include <string>
#include <vector>

namespace GVA {

/**
 * @brief This class represents model layer tensor - a piece of data that contains and describes model layer output
 */
class Tensor {
  public:
    /**
     * @brief This enum class describes model layer precision
     */
    enum class Precision {
        ANY = 0,   /**< default value */
        FP32 = 10, /**< 32bit floating point value */
        U8 = 40    /**< unsignned 8bit integer value */
    };

    /**
     * @brief This enum class describes model layer layout
     */
    enum class Layout {
        ANY = 0,  /**< unspecified layout */
        NCHW = 1, /**< NCWH layout */
        NHWC = 2, /**< NHWC layout */
    };

    /**
     * @brief Construct Tensor instance from GstStructure
     * @param s GstStructure to create Tensor instance from. It's assumed that tensor data is stored in "data_buffer"
     * field
     */
    Tensor(GstStructure *s) : s(s) {
    }

    /**
     * @brief Check if Tensor instance has field
     * @param field_name field name
     * @return True if field with this name is found, False otherwise
     */
    bool has_field(std::string field_name) {
        return gst_structure_has_field(s, field_name.data());
    }

    /**
     * @brief Get string contained in field_name's value
     * @param field_name field name
     * @return string with field_name's value if field_name is found and contains a string, empty string
     * otherwise
     */
    std::string get_string(std::string field_name) {
        const gchar *val = gst_structure_get_string(s, field_name.data());
        return (val) ? std::string(val) : std::string();
    }

    /**
     * @brief Get int contained in field_name's value
     * @param field_name field name
     * @return int with field_name's value if field_name is found and contains an int, 0 otherwise
     */
    int get_int(std::string field_name) {
        gint val = 0;
        gst_structure_get_int(s, field_name.data(), &val);
        return val;
    }

    /**
     * @brief Get double contained in field_name's value
     * @param field_name field name
     * @return double with field_name's value if field_name is found and contains an double, 0 otherwise
     */
    double get_double(std::string field_name) {
        double val = 0;
        gst_structure_get_double(s, field_name.data(), &val);
        return val;
    }

    /**
     * @brief Set field_name with string value
     * @param field_name field name
     * @param string value to set
     */
    void set_string(std::string field_name, std::string value) {
        gst_structure_set(s, field_name.data(), G_TYPE_STRING, value.data(), NULL);
    }

    /**
     * @brief Set field_name with int value
     * @param field_name field name
     * @param int value to set
     */
    void set_int(std::string field_name, int value) {
        gst_structure_set(s, field_name.data(), G_TYPE_INT, value, NULL);
    }

    /**
     * @brief Set field_name with double value
     * @param field_name field name
     * @param double value to set
     */
    void set_double(std::string field_name, double value) {
        gst_structure_set(s, field_name.data(), G_TYPE_DOUBLE, value, NULL);
    }

    /**
     * @brief Get tensor data
     * @tparam T type of tensor's elements
     * @return vector of values of type T containing tensor elements, empty vector if data can't be read
     */
    template <class T>
    const std::vector<T> data() {
        gsize size = 0;
        const void *data = gva_get_tensor_data(s, &size);
        if (!data || !size)
            return std::vector<T>();
        return std::vector<T>((T *)data, (T *)((char *)data + size));
    }

    /**
     * @brief Get name as a string
     * @return Tensor instance name
     */
    std::string name() {
        return gst_structure_get_name(s);
    }

    /**
     * @brief Get Precision
     * @return Precision, Precision::ANY if can't be read
     */
    Precision precision() {
        return (Precision)get_int("precision");
    }

    /**
     * @brief Get precision as a string
     * @return precision as a string, "ANY" if can't be read
     */
    std::string precision_as_string() {
        Precision precision_value = precision();
        switch (precision_value) {
        case Precision::U8:
            return "U8";
        case Precision::FP32:
            return "FP32";
        default:
            return "ANY";
        }
    }

    /**
     * @brief Get Layout
     * @return Layout, Layout::ANY if can't be read
     */
    Layout layout() {
        return (Layout)get_int("layout");
    }

    /**
     * @brief Get layout as a string
     * @return layout as a string, "ANY" if can't be read
     */
    std::string layout_as_string() {
        Layout layout_value = layout();
        switch (layout_value) {
        case Layout::NCHW:
            return "NCHW";
        case Layout::NHWC:
            return "NHWC";
        default:
            return "ANY";
        }
    }

    /**
     * @brief Get layer name
     * @return layer name as a string, empty string if failed to get
     */
    std::string layer_name() {
        return get_string("layer_name");
    }

    /**
     * @brief Get model name
     * @return model name as a string, empty string if failed to get
     */
    std::string model_name() {
        return get_string("model_name");
    }

    /**
     * @brief Get format
     * @return format as a string, empty string if failed to get
     */
    std::string format() {
        return get_string("format");
    }

    /**
     * @brief Get confidence
     * @return confidence as a double, 0 if failed to get
     */
    double confidence() {
        return get_double("confidence");
    }

    /**
     * @brief Get label
     * @return label as a string, empty string if failed to get
     */
    std::string label() {
        return get_string("label");
    }

    /**
     * @brief Get object id
     * @return object id as an int, 0 if failed to get
     */
    int object_id() {
        return get_int("object_id");
    }

    /**
     * @brief Get label id
     * @return label id as an int, 0 if failed to get
     */
    int label_id() {
        return get_int("label_id");
    }

    /**
     * @brief Get ptr to s
     * @return ptr to s
     */
    GstStructure *gst_structure() {
        return s;
    }

  protected:
    /**
     * @brief ptr to GstStructure that contains tensor
     */
    GstStructure *s;
};

/**
 * @brief This class represents region of interest - object with multiple tensors attached by multiple models (e.g.,
 * region of interest with detected face and recognized age and sex. It can be produced in a pipeline after gvainference
 * with detection model and gvaclassify with two models. Such ROI will have 3 tensors attached)
 */
class RegionOfInterest {
  protected:
    /**
     * @brief GstVideoRegionOfInterestMeta containing actual tensors
     */
    GstVideoRegionOfInterestMeta *gst_meta;
    /**
     * @brief Tensor instances that refer to tensors
     */
    std::vector<Tensor> tensors;
    /**
     * @brief Tensor instance that refers to detection tensor
     */
    Tensor *detection;

  public:
    /**
     * @brief Construct RegionOfInterest instance from GstVideoRegionOfInterestMeta
     * @param meta GstVideoRegionOfInterestMeta containing tensors
     */
    RegionOfInterest(GstVideoRegionOfInterestMeta *meta) : gst_meta(meta), detection(NULL) {
        g_return_if_fail(gst_meta != NULL);

        tensors.reserve(g_list_length(meta->params));

        for (GList *l = meta->params; l; l = g_list_next(l)) {
            GstStructure *s = (GstStructure *)l->data;
            tensors.push_back(Tensor(s));
            if (tensors.back().name() == "detection") {
                detection = &tensors.back();
            }
        }
    }

    /**
     * @brief Get number of tensors
     * @return number of tensors
     */
    int number_tensors() {
        return tensors.size();
    }

    /**
     * @brief Get tensor by index
     * @param index index to get tensor by
     * @return tensor by index
     */
    Tensor &operator[](int index) {
        return tensors[index];
    }

    /**
     * @brief Get ptr to gst_meta
     * @return ptr to gst_meta
     */
    GstVideoRegionOfInterestMeta *meta() {
        return gst_meta;
    }

    /**
     * @brief Get detection tensor confidence
     * @return detection tensor confidence if exists, otherwise 0
     */
    double confidence() {
        return detection ? detection->confidence() : 0;
    }
    typedef std::vector<Tensor>::iterator iterator;
    typedef std::vector<Tensor>::const_iterator const_iterator;

    /**
     * @brief Get iterator pointing to the first element in the tensors vector
     * @return iterator pointing to the first element in the tensors vector
     */
    iterator begin() {
        return tensors.begin();
    }

    /**
     * @brief Get iterator pointing to the past-the-end element in the tensors vector
     * @return iterator pointing to the past-the-end element in the tensors vector
     */
    iterator end() {
        return tensors.end();
    }
};

/**
 * @brief This class represents vector of regions of interest
 */
class RegionOfInterestList {
  protected:
    /**
     * @brief vector of RegionOfInterest instances
     */
    std::vector<RegionOfInterest> objects;

  public:
    /**
     * @brief Construct RegionOfInterestList instance from GstBuffer
     * @param buffer buffer that have GstVideoRegionOfInterestMeta instances attached
     */
    RegionOfInterestList(GstBuffer *buffer) {
        GstVideoRegionOfInterestMeta *meta = NULL;
        gpointer state = NULL;
        while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
            objects.push_back(RegionOfInterest(meta));
        }
    }

    /**
     * @brief Get number of objects
     * @return number of objects
     */
    int NumberObjects() {
        return objects.size();
    }

    /**
     * @brief Get object by index
     * @param index index to get object by
     * @return object by index
     */
    RegionOfInterest &operator[](int index) {
        return objects[index];
    }
    typedef std::vector<RegionOfInterest>::iterator iterator;
    typedef std::vector<RegionOfInterest>::const_iterator const_iterator;

    /**
     * @brief Get iterator pointing to the first element in the objects vector
     * @return iterator pointing to the first element in the objects vector
     */
    iterator begin() {
        return objects.begin();
    }

    /**
     * @brief Get iterator pointing to the past-the-end element in the objects vector
     * @return iterator pointing to the past-the-end element in the objects vector
     */
    iterator end() {
        return objects.end();
    }
};

} // namespace GVA

#endif // #ifdef __cplusplus
