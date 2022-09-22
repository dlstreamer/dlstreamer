/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "object_track.h"
#include "dlstreamer/gst/utils.h"
#include <algorithm>
#include <stdexcept>

/* Properties */
enum { PROP_0, PROP_SPATIAL_FEATURE, PROP_SPATIAL_FEATURE_DISTANCE };

enum class SpatialFeatureType {
    None,
    Histogram,
    SlicedHistogram,
    Inference,
};

static const GEnumValue SpatialFeatureValues[] = {
    {(int)SpatialFeatureType::None,
     "Spatial feature not used (only temporal features used, such as object shape and trajectory)", "none"},
    {(int)SpatialFeatureType::Histogram, "RGB histogram", "histogram"},
    {(int)SpatialFeatureType::SlicedHistogram, "RGB histogram on image divided into slices", "sliced-histogram"},
    {(int)SpatialFeatureType::Inference, "Inference on ReId model", "inference"},
    {0, NULL, NULL}};

typedef enum { None, Bhattacharyya, Cosine } SpatialFeatureDistanceType;

static const GEnumValue SpatialFeatureDistanceValues[] = {
    {(int)SpatialFeatureDistanceType::None, "Spatial feature not used", "none"},
    {(int)SpatialFeatureDistanceType::Bhattacharyya, "Bhattacharyya distance", "bhattacharyya"},
    {(int)SpatialFeatureDistanceType::Cosine, "Cosine distance", "cosine"},
    {0, NULL, NULL}};

typedef struct _ObjectTrack {
    VideoInference base;
    SpatialFeatureType spatial_feature;
    SpatialFeatureDistanceType spatial_feature_distance;
} ObjectTrack;

typedef struct _ObjectTrackClass {
    VideoInferenceClass base;
} ObjectTrackClass;

G_DEFINE_TYPE(ObjectTrack, object_track, GST_TYPE_VIDEO_INFERENCE);

#define OBJECT_TRACK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), object_track_get_type(), ObjectTrack))

GType spatial_feature_get_type(void) {
    static GType gtype = 0;
    if (!gtype)
        gtype = g_enum_register_static("SpatialFeatureType", SpatialFeatureValues);
    return gtype;
}

GType spatial_feature_distance_get_type(void) {
    static GType gtype = 0;
    if (!gtype)
        gtype = g_enum_register_static("SpatialFeatureDistanceType", SpatialFeatureDistanceValues);
    return gtype;
}

static void object_track_init(ObjectTrack *self) {
    self->spatial_feature = SpatialFeatureType::None;
    self->spatial_feature_distance = SpatialFeatureDistanceType::None;
}

void object_track_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    ObjectTrack *self = OBJECT_TRACK(object);
    switch (property_id) {
    case PROP_SPATIAL_FEATURE:
        g_value_set_enum(value, static_cast<gint>(self->spatial_feature));
        break;
    case PROP_SPATIAL_FEATURE_DISTANCE:
        g_value_set_enum(value, static_cast<gint>(self->spatial_feature_distance));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void object_track_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    ObjectTrack *self = OBJECT_TRACK(object);
    switch (property_id) {
    case PROP_SPATIAL_FEATURE:
        self->spatial_feature = static_cast<SpatialFeatureType>(g_value_get_enum(value));
        break;
    case PROP_SPATIAL_FEATURE_DISTANCE:
        self->spatial_feature_distance = static_cast<SpatialFeatureDistanceType>(g_value_get_enum(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static GstStateChangeReturn object_track_change_state(GstElement *element, GstStateChange transition) {
    auto self = OBJECT_TRACK(element);

    if (transition == GST_STATE_CHANGE_NULL_TO_READY) {
        if (self->spatial_feature == SpatialFeatureType::None) {
            splitjoinbin_set_elements_description(GST_SPLITJOINBIN(element), NULL, NULL, NULL, NULL,
                                                  "object_association_opencv");
            splitjoinbin_link_elements(GST_SPLITJOINBIN(element));
        } else {
            g_object_set(G_OBJECT(self), "inference-region", "roi-list", NULL);
            gst_util_set_object_arg(G_OBJECT(self), "inference-region", "roi-list"); // TODO

            // if DL model set, spatial_feature=inference
            if (!dlstreamer::get_property_as_string(G_OBJECT(element), "model").empty())
                self->spatial_feature = SpatialFeatureType::Inference;

            // process=tensor_histogram
            if (self->spatial_feature == SpatialFeatureType::Histogram ||
                self->spatial_feature == SpatialFeatureType::SlicedHistogram) {
                std::string elem = "tensor_histogram";

                if (dlstreamer::get_property_as_string(G_OBJECT(element), "device").rfind("GPU", 0) == 0) {
                    elem = "tensor_histogram_sycl";
                    gst_util_set_object_arg(G_OBJECT(self), "pre-process-backend", "vaapi-tensors");
                    gst_util_set_object_arg(G_OBJECT(self), "scale-method", "dls-vaapi"); // TODO not needed?
                }

                if (self->spatial_feature == SpatialFeatureType::SlicedHistogram)
                    elem += " num-slices-x=2 num-slices-y=2";

                self->base.set_inference_element(elem.data());
            } else if (self->spatial_feature != SpatialFeatureType::Inference) {
                throw std::runtime_error("Unknown spatial feature type");
            }

            // postaggregate=object_association_opencv
            if (dlstreamer::get_property_as_string(G_OBJECT(self), "postaggregate") == "NULL") {
                if (self->spatial_feature_distance == SpatialFeatureDistanceType::None)
                    self->spatial_feature_distance = (self->spatial_feature == SpatialFeatureType::Inference)
                                                         ? SpatialFeatureDistanceType::Cosine
                                                         : SpatialFeatureDistanceType::Bhattacharyya;
                std::string postaggregate =
                    "object_association_opencv spatial-feature-metadata-name=spatial-feature spatial-feature-distance=";
                for (auto &v : SpatialFeatureDistanceValues) {
                    if (v.value == self->spatial_feature_distance) {
                        postaggregate += v.value_nick;
                        break;
                    }
                }
                self->base.set_postaggregate_element(postaggregate.data());
            }
        }
    }

    return GST_ELEMENT_CLASS(object_track_parent_class)->change_state(element, transition);
}

static void object_track_class_init(ObjectTrackClass *klass) {
    auto gobject_class = G_OBJECT_CLASS(klass);
    auto element_class = GST_ELEMENT_CLASS(klass);
    gobject_class->get_property = object_track_get_property;
    gobject_class->set_property = object_track_set_property;
    element_class->change_state = GST_DEBUG_FUNCPTR(object_track_change_state);

    auto video_inference = VIDEO_INFERENCE_CLASS(klass);
    video_inference->get_default_postprocess_elements = [](VideoInference *) -> std::string {
        return "tensor_postproc_copy_params attribute-name=spatial-feature";
    };

    const auto kParamFlags = static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);
    g_object_class_install_property(
        gobject_class, PROP_SPATIAL_FEATURE,
        g_param_spec_enum("spatial-feature", "spatial-feature", "Spatial feature used by object tracking algorithm",
                          spatial_feature_get_type(), static_cast<gint>(SpatialFeatureType::None), kParamFlags));
    g_object_class_install_property(gobject_class, PROP_SPATIAL_FEATURE_DISTANCE,
                                    g_param_spec_enum("spatial-feature-distance", "spatial-feature-distance",
                                                      "Method to calculate distance between two spatial features",
                                                      spatial_feature_distance_get_type(),
                                                      static_cast<gint>(SpatialFeatureDistanceType::None),
                                                      kParamFlags));

    gst_element_class_set_metadata(element_class, OBJECT_TRACK_NAME, "video", OBJECT_TRACK_DESCRIPTION,
                                   "Intel Corporation");
}
