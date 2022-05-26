/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "preproc_opencv.hpp"

#include <capabilities/video_caps.hpp>
#include <i_preproc_elem.hpp>
#include <inference_backend/buffer_mapper.h>

#include <inference_backend/pre_proc.h>

#include <frame_data.hpp>
#include <safe_arithmetic.hpp>
#include <utils.h>

GST_DEBUG_CATEGORY(gva_preproc_opencv_debug_category);
#define GST_CAT_DEFAULT gva_preproc_opencv_debug_category

class GvaPreprocOpencvPrivate : public IPreProcElem {
  public:
    explicit GvaPreprocOpencvPrivate(GvaPreprocBase *base) : _base(base) {
    }

    virtual ~GvaPreprocOpencvPrivate() {
        if (_input_info)
            gst_video_info_free(_input_info);
        if (_output_info)
            gst_video_info_free(_output_info);
    }

    void init_preprocessing(const InferenceBackend::InputImageLayerDesc::Ptr &pre_proc_info, GstCaps *input_caps,
                            GstCaps *output_caps) final {
        _pre_proc_info = pre_proc_info;

        _input_info = gst_video_info_new();
        if (!gst_video_info_from_caps(_input_info, input_caps))
            throw std::runtime_error("Failed to get video info from in caps");

        _input_image_mapper = BufferMapperFactory::createMapper(InferenceBackend::MemoryType::SYSTEM, _input_info);

        _output_info = gst_video_info_new();
        if (!gst_video_info_from_caps(_output_info, output_caps))
            throw std::runtime_error("Failed to get video info from out caps");

        _output_image_mapper = BufferMapperFactory::createMapper(InferenceBackend::MemoryType::SYSTEM, _output_info);
    }

    bool need_preprocessing() const final {
        if (_pre_proc_info && _pre_proc_info->isDefined())
            return true;
        return !gst_video_info_is_equal(_input_info, _output_info);
    }

    GstFlowReturn run_preproc(GstBuffer *inbuf, GstBuffer *outbuf, GstVideoRegionOfInterestMeta *roi) const final {
        if (!inbuf || !outbuf) {
            GST_ERROR_OBJECT(_base, "OpenCVPreProc: GstBuffer is null");
            return GST_FLOW_ERROR;
        }

        try {
            auto src_image = _input_image_mapper->map(inbuf, GST_MAP_READ);
            auto dst_image = _output_image_mapper->map(outbuf, GST_MAP_WRITE);

            if (roi)
                src_image->rect = {roi->x, roi->y, roi->w, roi->h};
            else
                src_image->rect = {0, 0, src_image->width, src_image->height};
            dst_image->rect = {0, 0, dst_image->width, dst_image->height};

            auto vpp = std::unique_ptr<InferenceBackend::ImagePreprocessor>(
                InferenceBackend::ImagePreprocessor::Create(InferenceBackend::ImagePreprocessorType::OPENCV));
            vpp->Convert(*src_image, *dst_image, _pre_proc_info, nullptr, false);
        } catch (const std::exception &e) {
            GST_ERROR_OBJECT(_base, "Failure during preprocessing: %s", Utils::createNestedErrorMsg(e).c_str());
            return GST_FLOW_ERROR;
        }

        return GST_FLOW_OK;
    }

    gboolean transform_size(GstPadDirection /*direction*/, GstCaps * /*caps*/, gsize /*size*/, GstCaps * /*othercaps*/,
                            gsize *othersize) final {
        *othersize = _output_info->size;
        return true;
    }

  private:
    GvaPreprocBase *_base;
    InferenceBackend::InputImageLayerDesc::Ptr _pre_proc_info;
    std::unique_ptr<InferenceBackend::BufferToImageMapper> _input_image_mapper;
    std::unique_ptr<InferenceBackend::BufferToImageMapper> _output_image_mapper;
    GstVideoInfo *_input_info = nullptr;
    GstVideoInfo *_output_info = nullptr;
};

G_DEFINE_TYPE_EXTENDED(GvaPreprocOpencv, gva_preproc_opencv, GST_TYPE_GVA_PREPROC_BASE, 0,
                       G_ADD_PRIVATE(GvaPreprocOpencv);
                       GST_DEBUG_CATEGORY_INIT(gva_preproc_opencv_debug_category, "preproc_opencv", 0,
                                               "debug category for preproc_opencv element"));

static void gva_preproc_opencv_init(GvaPreprocOpencv *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // Initialization of private data
    auto *priv_memory = gva_preproc_opencv_get_instance_private(self);
    self->impl = new (priv_memory) GvaPreprocOpencvPrivate(&self->base);

    self->base.set_preproc_elem(self->impl);
}

static void gva_preproc_opencv_finalize(GObject *object) {
    auto self = GVA_PREPROC_OPENCV(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    if (self->impl) {
        self->impl->~GvaPreprocOpencvPrivate();
        self->impl = nullptr;
    }

    G_OBJECT_CLASS(gva_preproc_opencv_parent_class)->finalize(object);
}

static void gva_preproc_opencv_class_init(GvaPreprocOpencvClass *klass) {
    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = gva_preproc_opencv_finalize;

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, "[Preview] OpenCV Video Preprocessing Element", "application",
                                          "Performs preprocessing of a video input", "Intel Corporation");

    gst_element_class_add_pad_template(
        element_class,
        gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GST_VIDEO_CAPS_MAKE("{ BGR }"))));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(SYSTEM_MEM_CAPS)));
}

static gboolean plugin_init(GstPlugin *plugin) {
    return gst_element_register(plugin, "preproc_opencv", GST_RANK_NONE, GST_TYPE_GVA_PREPROC_OPENCV);
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, preproc_opencv,
                  PRODUCT_FULL_NAME " OpenCV preprocessing elements", plugin_init, PLUGIN_VERSION, PLUGIN_LICENSE,
                  PACKAGE_NAME, GST_PACKAGE_ORIGIN)
