/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/vaapi/elements/vaapi_batch_proc.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/utils.h"
#include "dlstreamer/vaapi/context.h"
#include "dlstreamer/vaapi/frame_alloc.h"
#include <climits>
#include <mutex>
#include <va/va_backend.h>

namespace dlstreamer {

namespace param {
static constexpr auto aspect_ratio = "aspect-ratio";
static constexpr auto output_format = "output-format";
}; // namespace param

static ParamDescVector params_desc = {
    {param::aspect_ratio, "Keep the aspect ratio (add borders if necessary)", false},
    {param::output_format, "Image format for output frames: BGR or RGB or GRAY", "BGR"},
};

class VaapiBatchProc : public BaseTransform {
  public:
    VaapiBatchProc(DictionaryCPtr params, const ContextPtr &app_context) : BaseTransform(app_context) {
        _aspect_ratio = params->get<bool>(param::aspect_ratio, false);
        std::string output_format_str = params->get(param::output_format, std::string());
        if (!output_format_str.empty()) {
            if (output_format_str.find("BGR") != std::string::npos)
                _output_format = ImageFormat::BGRX;
            else if (output_format_str.find("RGB") != std::string::npos)
                _output_format = ImageFormat::RGBX;
            else
                throw std::runtime_error("Unknown image format: " + output_format_str);
        }
    }

    bool init_once() override {
        _vaapi_context = VAAPIContext::create(_app_context);
        init_vaapi();
        create_mapper({_app_context, _vaapi_context});

        if (_output_info.format)
            _output_format = static_cast<ImageFormat>(_output_info.format);
        return true;
    }

    std::function<FramePtr()> get_output_allocator() override {
        return [this]() {
            FrameInfo output_info(_output_format, MemoryType::VAAPI, _output_info.tensors);
            return std::make_shared<VAAPIFrameAlloc>(output_info, _vaapi_context);
        };
    }

    FramePtr process(FramePtr src) override {
        std::lock_guard<std::mutex> guard(_mutex);
        DLS_CHECK(init());
        auto dst = create_output();
        ImageInfo src_info0(src->tensor(0)->info());
        ImageInfo dst_info0(dst->tensor(0)->info());
        uint16_t dst_w = dst_info0.width();
        uint16_t dst_h = dst_info0.height();

        int batch_size = src->num_tensors();
        // if format specified, it's multiple-plane image not batch
        if (src->media_type() == MediaType::Image && src->format())
            batch_size = 1;

        std::vector<VAProcPipelineParameterBuffer> pipeline_param_bufs(batch_size);
        std::vector<VABufferID> pipeline_param_buf_ids(batch_size);
        std::vector<VARectangle> src_rects(batch_size);
        std::vector<VARectangle> dst_rects(batch_size);

        for (int i = 0; i < batch_size; i++) {
            auto tensor = src->tensor(i).map<VAAPITensor>(_vaapi_context, AccessMode::Read);
            auto &info = src->tensor(i)->info();
            ImageInfo image_info(info);
            uint16_t src_w = image_info.width();
            uint16_t src_h = image_info.height();

            // input and output regions
            auto &src_rect = src_rects[i];
            auto &dst_rect = dst_rects[i];
            int16_t src_x = tensor->offset_x();
            int16_t src_y = tensor->offset_y();
            src_rect = {.x = src_x, .y = src_y, .width = src_w, .height = src_h};
            dst_rect = {.x = 0, .y = static_cast<int16_t>(i * dst_h), .width = dst_w, .height = dst_h};
            pipeline_param_bufs[i] = {};
            pipeline_param_bufs[i].surface = tensor->va_surface();
            pipeline_param_bufs[i].surface_region = nullptr;
            pipeline_param_bufs[i].surface_region = &src_rect;
            pipeline_param_bufs[i].output_region = &dst_rect;

            if (_aspect_ratio) {
                double scale_x = static_cast<double>(dst_rect.width) / src_rect.width;
                double scale_y = static_cast<double>(dst_rect.height) / src_rect.height;
                double scale = std::min(scale_x, scale_y);
                dst_rect.width = src_rect.width * scale;
                dst_rect.height = src_rect.height * scale;
            }

            VA_CALL(_va_vtable->vaCreateBuffer(_va_driver, _va_context_id, VAProcPipelineParameterBufferType,
                                               sizeof(pipeline_param_bufs[i]), 1, &pipeline_param_bufs[i],
                                               &pipeline_param_buf_ids[i]));

            // Store metadata with coefficients for src<>dst coordinates conversion
            auto affine_meta = dst->metadata().add(AffineTransformInfoMetadata::name);
            dst_rect.y -= i * dst_h;
            AffineTransformInfoMetadata(affine_meta).set_rect(src_w, src_h, dst_w, dst_h, src_rect, dst_rect);
        }

        VAAPIFramePtr dst_vaapi = ptr_cast<VAAPIFrame>(dst);
        VASurfaceID dst_surface = dst_vaapi->va_surface();

        VA_CALL(_va_vtable->vaBeginPicture(_va_driver, _va_context_id, dst_surface));
        VA_CALL(_va_vtable->vaRenderPicture(_va_driver, _va_context_id, pipeline_param_buf_ids.data(), batch_size));
        VA_CALL(_va_vtable->vaEndPicture(_va_driver, _va_context_id));

        for (int i = 0; i < batch_size; i++) {
            VA_CALL(_va_vtable->vaDestroyBuffer(_va_driver, pipeline_param_buf_ids[i]));
        }

        return dst;
    }

  protected:
    bool _aspect_ratio = false;
    ImageFormat _output_format = ImageFormat::BGRX;
    VAAPIContextPtr _vaapi_context;
    VADriverContextP _va_driver = nullptr;
    VADriverVTable *_va_vtable = nullptr;
    VAConfigID _va_config_id = VA_INVALID_ID;
    VAContextID _va_context_id = VA_INVALID_ID;
    std::mutex _mutex;

    void init_vaapi() {
        auto _va_display = reinterpret_cast<VADisplayContextP>(_vaapi_context->va_display());
        _va_driver = _va_display->pDriverContext;
        _va_vtable = _va_display->pDriverContext->vtable;

        VAConfigAttrib attrib;
        attrib.type = VAConfigAttribRTFormat;
        attrib.value = VA_RT_FORMAT_YUV420; // TODO is VAConfigAttrib needed?
        VA_CALL(
            _va_vtable->vaCreateConfig(_va_driver, VAProfileNone, VAEntrypointVideoProc, &attrib, 1, &_va_config_id));
        VA_CALL(
            _va_vtable->vaCreateContext(_va_driver, _va_config_id, 0, 0, VA_PROGRESSIVE, nullptr, 0, &_va_context_id));
    }

    FrameInfo set_info_types(FrameInfo info, const FrameInfo &static_info) {
        info.media_type = static_info.media_type;
        info.memory_type = static_info.memory_type;
        info.format = static_info.format;
        return info;
    }
};

extern "C" {
ElementDesc vaapi_batch_proc = {.name = "vaapi_batch_proc",
                                .description = "Batched pre-processing with VAAPI memory as input and output",
                                .author = "Intel Corporation",
                                .params = nullptr,
                                .input_info = {{MediaType::Image, MemoryType::VAAPI}},
                                .output_info = {{MediaType::Tensors, MemoryType::VAAPI}},
                                .create = create_element<VaapiBatchProc>,
                                .flags = ELEMENT_FLAG_SHARABLE};
}

} // namespace dlstreamer
