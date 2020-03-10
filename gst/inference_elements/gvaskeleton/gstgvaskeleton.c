#include "gstgvaskeleton.h"
#include "gvaskeleton.h"
#include "gvaskeleton_caps.h"
#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>

#include "config.h"

#define UNUSED(x) (void)(x)

#define ELEMENT_LONG_NAME "Human Pose Estimation"
#define ELEMENT_DESCRIPTION "Human Pose Estimation"

GST_DEBUG_CATEGORY_STATIC(gst_gva_skeleton_debug_category);
#define GST_CAT_DEFAULT gst_gva_skeleton_debug_category

enum { PROP_0, PROP_MODEL_PATH, PROP_DEVICE, PROP_HANDS_DETECT };

static void gst_gva_skeleton_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_gva_skeleton_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void gst_gva_skeleton_dispose(GObject *object);
static void gst_gva_skeleton_finalize(GObject *object);
static void gst_gva_skeleton_cleanup(GstGvaSkeleton *gvaskeleton);

static gboolean gst_gva_skeleton_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);
static GstFlowReturn gst_gva_skeleton_transform_ip(GstBaseTransform *trans, GstBuffer *buf);

static gboolean gst_gva_skeleton_stop(GstBaseTransform *trans);
static gboolean gst_gva_skeleton_start(GstBaseTransform *base);

G_DEFINE_TYPE_WITH_CODE(GstGvaSkeleton, gst_gva_skeleton, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_skeleton_debug_category, "skeleton", 0,
                                                "debug category for skeleton element"));

void gst_gva_skeleton_class_init(GstGvaSkeletonClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);

    gobject_class->finalize = gst_gva_skeleton_finalize;
    gobject_class->set_property = gst_gva_skeleton_set_property;
    gobject_class->get_property = gst_gva_skeleton_get_property;
    gobject_class->dispose = GST_DEBUG_FUNCPTR(gst_gva_skeleton_dispose);

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");
    g_object_class_install_property(gobject_class, PROP_MODEL_PATH,
                                    g_param_spec_string("model_path",         // name
                                                        "model path",         // nickname
                                                        "Path to model path", // description
                                                        "",                   // default
                                                        G_PARAM_WRITABLE      // flags
                                                        ));
    g_object_class_install_property(gobject_class, PROP_DEVICE,
                                    g_param_spec_string("device",        // name
                                                        "device",        // nickname
                                                        "infer device",  // description
                                                        "CPU",           // default
                                                        G_PARAM_WRITABLE // flags
                                                        ));

    g_object_class_install_property(gobject_class, PROP_HANDS_DETECT,
                                    g_param_spec_boolean("hands-detect", "Hands-Detect", "Detection hands position.",
                                                         FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    base_transform_class->set_caps = GST_DEBUG_FUNCPTR(gst_gva_skeleton_set_caps);
    base_transform_class->transform = NULL;
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_gva_skeleton_transform_ip);
    base_transform_class->start = GST_DEBUG_FUNCPTR(gst_gva_skeleton_start);
    base_transform_class->stop = GST_DEBUG_FUNCPTR(gst_gva_skeleton_stop);
}

void gst_gva_skeleton_init(GstGvaSkeleton *skeleton) {
    GST_INFO_OBJECT(skeleton, "Initializing plugin");

    skeleton->is_initialized = FALSE;
}

void gst_gva_skeleton_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GstGvaSkeleton *skeleton = GST_GVA_SKELETON(object);

    GST_DEBUG_OBJECT(skeleton, "set_property");

    switch (prop_id) {
    case PROP_MODEL_PATH:
        skeleton->model_path = g_value_dup_string(value);
        break;
    case PROP_DEVICE:
        skeleton->device = g_value_dup_string(value);
        break;
    case PROP_HANDS_DETECT:
        skeleton->hands_detect = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

void gst_gva_skeleton_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstGvaSkeleton *skeleton = GST_GVA_SKELETON(object);

    GST_DEBUG_OBJECT(skeleton, "get_property");

    switch (prop_id) {
    case PROP_MODEL_PATH:
        g_value_set_string(value, skeleton->model_path);
        break;
    case PROP_DEVICE:
        g_value_set_string(value, skeleton->device);
        break;
    case PROP_HANDS_DETECT:
        g_value_set_boolean(value, skeleton->hands_detect);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

void gst_gva_skeleton_cleanup(GstGvaSkeleton *skeleton) {
    if (skeleton == NULL)
        return;

    GST_DEBUG_OBJECT(skeleton, "gst_gva_skeleton_cleanup");

    if (skeleton->model_path) {
        g_free(skeleton->model_path);
        skeleton->model_path = NULL;
    }
    if (skeleton->device) {
        g_free(skeleton->device);
        skeleton->device = NULL;
    }
    if (skeleton->hpe_object) {
        hpe_release(skeleton->hpe_object);
        skeleton->hpe_object = NULL;
    }
}

void gst_gva_skeleton_dispose(GObject *object) {
    GstGvaSkeleton *gvaskeleton = GST_GVA_SKELETON(object);

    GST_DEBUG_OBJECT(gvaskeleton, "dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gst_gva_skeleton_parent_class)->dispose(object);
}

void gst_gva_skeleton_finalize(GObject *object) {
    GstGvaSkeleton *skeleton = GST_GVA_SKELETON(object);

    GST_DEBUG_OBJECT(skeleton, "finalize");

    gst_gva_skeleton_cleanup(skeleton);

    G_OBJECT_CLASS(gst_gva_skeleton_parent_class)->finalize(object);
}

gboolean gst_gva_skeleton_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps) {
    UNUSED(outcaps);

    GstGvaSkeleton *gvaskeleton = GST_GVA_SKELETON(trans);

    GST_DEBUG_OBJECT(gvaskeleton, "set_caps");

    gst_video_info_from_caps(&gvaskeleton->info, incaps);

    return TRUE;
}

GstFlowReturn gst_gva_skeleton_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
    GstGvaSkeleton *skeleton = GST_GVA_SKELETON(trans);

    GST_DEBUG_OBJECT(skeleton, "transform_ip");

    if (!gst_pad_is_linked(GST_BASE_TRANSFORM_SRC_PAD(trans))) {
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }

    GvaSkeletonStatus status = hpe_to_estimate(skeleton->hpe_object, buf, skeleton->hands_detect, &skeleton->info);
    if (status == GVA_SKELETON_OK)
        return GST_FLOW_OK;

    return GST_FLOW_ERROR;
}

gboolean gst_gva_skeleton_start(GstBaseTransform *base) {
    GstGvaSkeleton *skeleton = GST_GVA_SKELETON(base);
    GST_INFO_OBJECT(skeleton, "Start");

    if (skeleton->model_path == NULL) {
        g_error("'model_path' is set to null");
    } else if (!g_file_test(skeleton->model_path, G_FILE_TEST_EXISTS)) {
        g_error("path %s set in 'model_path' does not exist", skeleton->model_path);
    }

    if (!skeleton->is_initialized) {
        skeleton->hpe_object = hpe_initialization(skeleton->model_path, "CPU");
        if (!skeleton->hpe_object)
            g_error("Human pose estimator initialization is failed.");
        skeleton->is_initialized = TRUE;
    }

    GST_INFO_OBJECT(skeleton, "Start is successfull");
    return TRUE;
}

gboolean gst_gva_skeleton_stop(GstBaseTransform *trans) {
    GstGvaSkeleton *gvaskeleton = GST_GVA_SKELETON(trans);

    GST_DEBUG_OBJECT(gvaskeleton, "stop");

    return TRUE;
}