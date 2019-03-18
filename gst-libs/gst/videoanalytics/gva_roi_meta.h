/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>
#include <gst/video/gstvideometa.h>

#ifdef __cplusplus
extern "C" {
#endif
inline const void *gva_get_tensor_data(GstStructure *s, gsize *nbytes) {
    const GValue *f = gst_structure_get_value(s, "data_buffer");
    if (!f)
        return NULL;
    GVariant *v = g_value_get_variant(f);
    return g_variant_get_fixed_array(v, nbytes, 1);
}
#ifdef __cplusplus
}
#endif

#define GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buf, state)                                                          \
    ((GstVideoRegionOfInterestMeta *)_gst_buffer_iterate_meta_filtered(buf, state,                                     \
                                                                       GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE))

#if defined(GST_VERSION_MAJOR) && (GST_VERSION_MAJOR >= 1 && GST_VERSION_MINOR >= 12)
#define _gst_buffer_iterate_meta_filtered gst_buffer_iterate_meta_filtered
#else
inline GstMeta *_gst_buffer_iterate_meta_filtered(GstBuffer *gst_buffer, gpointer *state_ptr,
                                                  GType meta_api_type_filter) {
    GstMeta *meta = NULL;
    while (meta = gst_buffer_iterate_meta(gst_buffer, state_ptr))
        if (meta->info->api == meta_api_type_filter)
            break;
    return meta;
}
#endif

#ifdef __cplusplus

#include <string>
#include <vector>

namespace GVA {

class Tensor {
  public:
    enum class Precision { ANY = 0, FP32 = 10, U8 = 40 };

    enum class Layout {
        ANY = 0,
        NCHW = 1,
        NHWC = 2,
    };

    Tensor(GstStructure *s) : s(s) {
    }

    bool has_field(std::string field_name) {
        return gst_structure_has_field(s, field_name.data());
    }

    std::string get_string(std::string field_name) {
        const gchar *val = gst_structure_get_string(s, field_name.data());
        return (val) ? std::string(val) : std::string();
    }

    int get_int(std::string field_name) {
        gint val = 0;
        gst_structure_get_int(s, field_name.data(), &val);
        return val;
    }

    double get_double(std::string field_name) {
        double val = 0;
        gst_structure_get_double(s, field_name.data(), &val);
        return val;
    }

    void set_string(std::string field_name, std::string value) {
        gst_structure_set(s, field_name.data(), G_TYPE_STRING, value.data(), NULL);
    }

    void set_int(std::string field_name, int value) {
        gst_structure_set(s, field_name.data(), G_TYPE_INT, value, NULL);
    }

    void set_double(std::string field_name, double value) {
        gst_structure_set(s, field_name.data(), G_TYPE_DOUBLE, value, NULL);
    }

    // Known fields
    template <class T>
    const std::vector<T> data() {
        gsize size = 0;
        const void *data = gva_get_tensor_data(s, &size);
        if (!data || !size)
            return std::vector<T>();
        return std::vector<T>((T *)data, (T *)((char *)data + size));
    }

    std::string name() {
        return gst_structure_get_name(s);
    }

    Precision precision() {
        return (Precision)get_int("precision");
    }
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

    Layout layout() {
        return (Layout)get_int("layout");
    }
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

    std::string layer_name() {
        return get_string("layer_name");
    }

    std::string model_name() {
        return get_string("model_name");
    }

    std::string format() {
        return get_string("format");
    }

    double confidence() {
        return get_double("confidence");
    }

    std::string label() {
        return get_string("label");
    }

    int object_id() {
        return get_int("object_id");
    }

    int label_id() {
        return get_int("label_id");
    }

    GstStructure *gst_structure() {
        return s;
    }

  protected:
    GstStructure *s;
};

class RegionOfInterest {
  protected:
    GstVideoRegionOfInterestMeta *gst_meta;
    std::vector<Tensor> tensors;
    Tensor *detection;

  public:
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
    int number_tensors() {
        return tensors.size();
    }
    Tensor &operator[](int index) {
        return tensors[index];
    }
    GstVideoRegionOfInterestMeta *meta() {
        return gst_meta;
    }
    double confidence() {
        return detection ? detection->confidence() : 0;
    }
    typedef std::vector<Tensor>::iterator iterator;
    typedef std::vector<Tensor>::const_iterator const_iterator;
    iterator begin() {
        return tensors.begin();
    }
    iterator end() {
        return tensors.end();
    }
};

class RegionOfInterestList {
  protected:
    std::vector<RegionOfInterest> objects;

  public:
    RegionOfInterestList(GstBuffer *buffer) {
        GstVideoRegionOfInterestMeta *meta = NULL;
        gpointer state = NULL;
        while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
            objects.push_back(RegionOfInterest(meta));
        }
    }
    int NumberObjects() {
        return objects.size();
    }
    RegionOfInterest &operator[](int index) {
        return objects[index];
    }
    typedef std::vector<RegionOfInterest>::iterator iterator;
    typedef std::vector<RegionOfInterest>::const_iterator const_iterator;
    iterator begin() {
        return objects.begin();
    }
    iterator end() {
        return objects.end();
    }
};

} // namespace GVA

#endif // #ifdef __cplusplus
