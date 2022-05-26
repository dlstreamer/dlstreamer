/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "preproc_vaapi.hpp"

#include <capabilities/video_caps.hpp>
#include <i_preproc_elem.hpp>

#include <dlstreamer/gst/vaapi_context.h>
#include <dlstreamer/vaapi/context.h>
#include <frame_data.hpp>
#include <inference_backend/buffer_mapper.h>
#include <inference_backend/pre_proc.h>
#include <opencv_utils/opencv_utils.h>
#include <safe_arithmetic.hpp>
#include <utils.h>

#include <vaapi_converter.h>
#include <vaapi_image_info.hpp>
#include <vaapi_images.h>

GST_DEBUG_CATEGORY(gva_preproc_vaapi_debug_category);
#define GST_CAT_DEFAULT gva_preproc_vaapi_debug_category

namespace GlobalUtils = Utils;
namespace IEUtils = InferenceBackend::Utils;
using namespace InferenceBackend;

namespace {

constexpr size_t _VA_IMAGE_POOL_SIZE = 5;

dlstreamer::VAAPIContextPtr createVaDisplay(GstBaseTransform *base_transform) {
    assert(base_transform);

    std::shared_ptr<dlstreamer::GSTVAAPIContext> display;
    try {
        display = std::make_shared<dlstreamer::GSTVAAPIContext>(base_transform);
        GST_DEBUG_OBJECT(base_transform, "Using shared VADisplay");
        return display;
    } catch (...) {
        GST_DEBUG_OBJECT(base_transform, "Error creating GSTVAAPIContext");
    }

    return vaApiCreateVaDisplay();
}

} // namespace

class GvaPreprocVaapiPrivate : public IPreProcElem {
  public:
    GvaPreprocVaapiPrivate(GvaPreprocBase *base) : _base(base) {
    }

    virtual ~GvaPreprocVaapiPrivate() {
        if (_input_info)
            gst_video_info_free(_input_info);
        if (_output_info)
            gst_video_info_free(_output_info);
    }

    void init_preprocessing(const InputImageLayerDesc::Ptr &pre_proc_info, GstCaps *input_caps,
                            GstCaps *output_caps) final {
        _pre_proc_info = pre_proc_info;
        _input_info = gst_video_info_new();
        if (!gst_video_info_from_caps(_input_info, input_caps))
            throw std::runtime_error("Failed to get video info from in caps");

        _output_info = gst_video_info_new();
        if (!gst_video_info_from_caps(_output_info, output_caps))
            throw std::runtime_error("Failed to get video info from out caps");

        _out_mem_type = get_memory_type_from_caps(output_caps);

        _va_context.reset(new VaApiContext(createVaDisplay(GST_BASE_TRANSFORM_CAST(_base))));
        _va_converter.reset(new VaApiConverter(_va_context.get()));
        _va_image_pool.reset(new VaApiImagePool(
            _va_context.get(), VaApiImagePool::SizeParams(_VA_IMAGE_POOL_SIZE),
            VaApiImagePool::ImageInfo{static_cast<uint32_t>(GST_VIDEO_INFO_WIDTH(_output_info)),
                                      static_cast<uint32_t>(GST_VIDEO_INFO_HEIGHT(_output_info)), 1,
                                      gst_format_to_four_CC(GST_VIDEO_INFO_FORMAT(_output_info)), _out_mem_type}));

        auto context = std::make_shared<dlstreamer::VAAPIContext>(_va_context->DisplayRaw());
        _image_mapper = BufferMapperFactory::createMapper(InferenceBackend::MemoryType::VAAPI, _input_info, context);

        _buffer_mapper = BufferMapperFactory::createMapper(InferenceBackend::MemoryType::SYSTEM);
    }

    bool need_preprocessing() const final {
        if (_pre_proc_info && _pre_proc_info->isDefined())
            return true;
        return !gst_video_info_is_equal(_input_info, _output_info);
    }

    GstFlowReturn run_preproc(GstBuffer *inbuf, GstBuffer *outbuf,
                              GstVideoRegionOfInterestMeta *roi = nullptr) const final {
        if (!inbuf || !outbuf) {
            GST_ERROR_OBJECT(_base, "VAAPIPreProc: GstBuffer is null");
            return GST_FLOW_ERROR;
        }

        try {
            auto src_image = _image_mapper->map(inbuf, GST_MAP_READ);

            VaApiImage *dst_image = _va_image_pool->AcquireBuffer();

            if (roi)
                src_image->rect = {roi->x, roi->y, roi->w, roi->h};
            else
                src_image->rect = {0, 0, src_image->width, src_image->height};

            _va_converter->Convert(*src_image, *dst_image, _pre_proc_info);

            if (_out_mem_type == MemoryType::SYSTEM) {
                auto gbuffer = std::make_shared<dlstreamer::GSTBuffer>(outbuf, _output_info);
                auto dst = _buffer_mapper->map(gbuffer, dlstreamer::AccessMode::WRITE);
                const auto &plane = dst->info()->planes.front();

                cv::Mat dst_mat;
                auto dst_mapped_image = dst_image->Map();
                IEUtils::ImageToMat(dst_mapped_image, dst_mat);

                cv::Mat out(plane.height(), plane.width(), CV_8UC3, static_cast<uint8_t *>(dst->data(0)),
                            plane.width_stride());
                switch (dst_mapped_image.format) {
                case FOURCC_BGRA:
                case FOURCC_BGRX:
                    cv::cvtColor(dst_mat, out, cv::COLOR_BGRA2BGR);
                    break;
                case FOURCC_BGR:
                    dst_mat.copyTo(out);
                    break;
                default:
                    throw std::runtime_error("Unsupported color format got from VAAPI to convert to BGR");
                }

                dst_image->Unmap();
                _va_image_pool->ReleaseBuffer(dst_image);
            } else {
                auto info = new VaapiImageInfo{_va_image_pool, dst_image, {}};
                dst_image->sync = info->sync.get_future();
                gst_mini_object_set_qdata(&outbuf->mini_object, g_quark_from_static_string("VaApiImage"), info,
                                          [](gpointer data) {
                                              auto info = reinterpret_cast<VaapiImageInfo *>(data);
                                              if (!info)
                                                  return;
                                              if (info->image)
                                                  info->image->Unmap();
                                              if (info->pool)
                                                  info->pool->ReleaseBuffer(info->image);
                                              info->sync.set_value();
                                              delete info;
                                          });
            }
        } catch (const std::exception &e) {
            GST_ERROR_OBJECT(_base, "Failure during preprocessing: %s", GlobalUtils::createNestedErrorMsg(e).c_str());
            return GST_FLOW_ERROR;
        }

        return GST_FLOW_OK;
    }

    gboolean transform_size(GstPadDirection /*direction*/, GstCaps * /*caps*/, gsize /*size*/, GstCaps * /*othercaps*/,
                            gsize *othersize) final {
        if (_out_mem_type == MemoryType::SYSTEM)
            *othersize = _output_info->size;
        else
            *othersize = 0;
        return true;
    }

    void flush() final {
        _va_image_pool->Flush();
    }

  private:
    GvaPreprocBase *_base;
    InputImageLayerDesc::Ptr _pre_proc_info;
    GstVideoInfo *_input_info = nullptr;
    GstVideoInfo *_output_info = nullptr;
    MemoryType _out_mem_type = MemoryType::ANY;
    std::unique_ptr<InferenceBackend::BufferToImageMapper> _image_mapper;
    dlstreamer::BufferMapperPtr _buffer_mapper;

    std::unique_ptr<VaApiContext> _va_context;
    std::unique_ptr<VaApiConverter> _va_converter;
    std::shared_ptr<VaApiImagePool> _va_image_pool;
};

G_DEFINE_TYPE_EXTENDED(GvaPreprocVaapi, gva_preproc_vaapi, GST_TYPE_GVA_PREPROC_BASE, 0, G_ADD_PRIVATE(GvaPreprocVaapi);
                       GST_DEBUG_CATEGORY_INIT(gva_preproc_vaapi_debug_category, "preproc_vaapi", 0,
                                               "debug category for preproc_vaapi element"));

static void gva_preproc_vaapi_init(GvaPreprocVaapi *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // Initialization of private data
    auto *priv_memory = gva_preproc_vaapi_get_instance_private(self);
    self->impl = new (priv_memory) GvaPreprocVaapiPrivate(&self->base);

    self->base.set_preproc_elem(self->impl);
}

static void gva_preproc_vaapi_finalize(GObject *object) {
    auto self = GVA_PREPROC_VAAPI(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    if (self->impl) {
        self->impl->~GvaPreprocVaapiPrivate();
        self->impl = nullptr;
    }

    G_OBJECT_CLASS(gva_preproc_vaapi_parent_class)->finalize(object);
}

static void gva_preproc_vaapi_class_init(GvaPreprocVaapiClass *klass) {
    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = gva_preproc_vaapi_finalize;

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, "[Preview] VAAPI Video Preprocessing Element", "application",
                                          "Performs preprocessing of a video input", "Intel Corporation");

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                                            gst_caps_from_string(GST_VIDEO_CAPS_MAKE("{ BGR }") ";" VASURFACE_CAPS)));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(VASURFACE_CAPS)));
}

static gboolean plugin_init(GstPlugin *plugin) {
    return gst_element_register(plugin, "preproc_vaapi", GST_RANK_NONE, GST_TYPE_GVA_PREPROC_VAAPI);
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, preproc_vaapi,
                  PRODUCT_FULL_NAME " VAAPI preprocessing elements", plugin_init, PLUGIN_VERSION, PLUGIN_LICENSE,
                  PACKAGE_NAME, GST_PACKAGE_ORIGIN)
