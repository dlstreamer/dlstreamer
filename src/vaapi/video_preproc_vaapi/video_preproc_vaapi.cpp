/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "video_preproc_vaapi.h"
#include "dlstreamer/utils.h"
#include <iostream>

namespace dlstreamer {

namespace param {
static constexpr auto batch_size = "batch_size";
}; // namespace param

ParamDescVector *VideoPreprocVAAPIParamsDesc() {
    static ParamDescVector params_desc = {
        // if batch_size=0, it selected during output caps negotiation
        {param::batch_size, "Batch size (0=autoselection)", 0, 0, INT_MAX},
    };
    return &params_desc;
};

#define VA_CALL(_FUNC)                                                                                                 \
    {                                                                                                                  \
        /*ITT_TASK(#_FUNC);*/                                                                                          \
        VAStatus _status = _FUNC;                                                                                      \
        if (_status != VA_STATUS_SUCCESS) {                                                                            \
            throw std::runtime_error(#_FUNC " failed, sts=" + std::to_string(_status));                                \
        }                                                                                                              \
    }

// Buffer that creates and controls lifetime of VASurface
class VAAPIBufferEx final : public VAAPIBuffer {
    VASurfaceID _surface;
    VAAPIContextPtr _va_context; // FIXME: move to VAAPIBuffer

  public:
    VAAPIBufferEx(VASurfaceID surface, BufferInfoCPtr info, ContextPtr context)
        : VAAPIBuffer(surface, info, context), _surface(surface) {
        _va_context = std::dynamic_pointer_cast<VAAPIContext>(context);
        if (!_va_context)
            throw std::runtime_error("failed to create VAAPIBufferEx: empty VAAPIContext");
    }

    ~VAAPIBufferEx() {
        VADriverContext *va_driver = unpack_drv_context(_va_context->va_display());
        VAStatus status = va_driver->vtable->vaDestroySurfaces(va_driver, &_surface, 1);
        // FIXME: logging system
        if (status != VA_STATUS_SUCCESS)
            std::cout << "vaDestroySurfaces() failed with status = " << status << std::endl;
    }

    static std::shared_ptr<VAAPIBufferEx> create(BufferInfoCPtr info, VAAPIContextPtr context, int rt_format) {
        auto surface = create_surface(info, context, rt_format);
        return std::make_shared<VAAPIBufferEx>(surface, std::move(info), std::move(context));
    }

    static VASurfaceID create_surface(BufferInfoCPtr info, VAAPIContextPtr context, int rt_format) {
        VASurfaceAttrib surface_attr = {};
        surface_attr.type = VASurfaceAttribPixelFormat;
        surface_attr.flags = VA_SURFACE_ATTRIB_SETTABLE;
        surface_attr.value.type = VAGenericValueTypeInteger;
        surface_attr.value.value.i = info->format;

        const auto &p0 = info->planes.front();
        unsigned int va_surface = VA_INVALID_SURFACE;
        auto va_driver = unpack_drv_context(context->va_display());
        auto vtable = va_driver->vtable;
        VA_CALL(
            vtable->vaCreateSurfaces2(va_driver, rt_format, p0.width(), p0.height(), &va_surface, 1, &surface_attr, 1));
        return va_surface;
    }

    static VADriverContext *unpack_drv_context(VADisplay va_display) {
        return reinterpret_cast<VADisplayContextP>(va_display)->pDriverContext;
    }
};

//
// VideoPreprocVAAPI implementation
//

VideoPreprocVAAPI::VideoPreprocVAAPI(ITransformController &transform_ctrl, DictionaryCPtr params)
    : TransformWithAlloc(transform_ctrl, std::move(params)) {
    _batch_size = _params->get<int>(param::batch_size);
}

void VideoPreprocVAAPI::init_vaapi() {
    auto _va_display = reinterpret_cast<VADisplayContextP>(_vaapi_context->va_display());
    _va_driver = _va_display->pDriverContext;
    _va_vtable = _va_display->pDriverContext->vtable;

    VAConfigAttrib attrib;
    attrib.type = VAConfigAttribRTFormat;
    attrib.value = RT_FORMAT;
    VA_CALL(_va_vtable->vaCreateConfig(_va_driver, VAProfileNone, VAEntrypointVideoProc, &attrib, 1, &_va_config_id));
    if (_va_config_id == VA_INVALID_ID) {
        throw std::invalid_argument("Could not create VA config. Cannot initialize VaApiContext without VA config.");
    }
    VA_CALL(_va_vtable->vaCreateContext(_va_driver, _va_config_id, 0, 0, VA_PROGRESSIVE, nullptr, 0, &_va_context_id));
    if (_va_context_id == VA_INVALID_ID) {
        throw std::invalid_argument("Could not create VA context. Cannot initialize VaApiContext without VA context.");
    }
}

BufferInfoVector VideoPreprocVAAPI::get_input_info(const BufferInfo &output_info) {
    if (output_info.planes.empty()) {
        return _desc->input_info;
    } else {
        BufferInfo input_info = set_info_types(output_info, _desc->input_info[0]);
        // remove batch_size from tensor shape (NHWC to HWC)
        for (auto &plane : input_info.planes) {
            std::vector<size_t> shape = plane.shape;
            if (!_batch_size)
                _batch_size = shape[0];
            else if (shape[0] != _batch_size)
                throw std::runtime_error("Expect batch_size on first dimension");
            shape.erase(shape.begin());
            plane = PlaneInfo(shape, plane.type, plane.name);
        }
        return {input_info};
    }
}

BufferInfoVector VideoPreprocVAAPI::get_output_info(const BufferInfo &input_info) {
    if (input_info.planes.empty()) {
        return _desc->output_info;
    } else {
        BufferInfo output_info = set_info_types(input_info, _desc->output_info[0]);
        output_info.format = input_info.format;
        // add batch_size to tensor shape (HWC to NHWC)
        for (auto &plane : output_info.planes) {
            std::vector<size_t> shape = plane.shape;
            shape.insert(shape.begin(), _batch_size ? _batch_size : 1);
            plane = PlaneInfo(shape, plane.type, plane.name);
        }
        return {output_info};
    }
}

void VideoPreprocVAAPI::set_info(const BufferInfo &input_info, const BufferInfo &output_info) {
    _input_info = input_info;
    _output_info = output_info;
    _vaapi_context = _transform_ctrl->get_context<VAAPIContext>();
    if (!_vaapi_context)
        throw std::runtime_error("Can't query VAAPI context ");

    _input_mapper = _transform_ctrl->create_input_mapper(BufferType::VAAPI_SURFACE, _vaapi_context);
    init_vaapi();
    _src_batch.reserve(_batch_size);
}

std::function<BufferPtr()> VideoPreprocVAAPI::get_output_allocator() {
    return [this]() {
        auto info = std::make_shared<BufferInfo>(_output_info);
        return VAAPIBufferEx::create(std::move(info), _vaapi_context, RT_FORMAT);
    };
}

BufferMapperPtr VideoPreprocVAAPI::get_output_mapper() {
    return nullptr; // std::make_shared<BufferMapperVAAPIToCPU>();
}

bool VideoPreprocVAAPI::process(BufferPtr src, BufferPtr dst) {
    std::lock_guard<std::mutex> guard(_mutex);

    _src_batch.push_back(src);
    if (_src_batch.size() < _batch_size) {
        return false;
    }

    auto dst_width = dst->info()->planes[0].width();
    auto dst_height = dst->info()->planes[0].height();

    std::vector<VAAPIBufferPtr> vaapi_buffers;
    for (size_t i = 0; i < _src_batch.size(); i++) {
        auto src_vaapi = _input_mapper->map<VAAPIBuffer>(_src_batch[i], AccessMode::READ);
        vaapi_buffers.push_back(src_vaapi);
    }

    std::vector<VAProcPipelineParameterBuffer> pipeline_param_bufs(_batch_size);
    std::vector<VABufferID> pipeline_param_buf_ids(_batch_size);
    std::vector<VARectangle> output_regions(_batch_size);

    for (size_t i = 0; i < _batch_size; i++) {
        pipeline_param_bufs[i] = {};
        pipeline_param_bufs[i].surface = vaapi_buffers[i]->va_surface();
        pipeline_param_bufs[i].output_region = &output_regions[i];
        output_regions[i] = {.x = 0,
                             .y = static_cast<int16_t>(i * dst_height),
                             .width = static_cast<uint16_t>(dst_width),
                             .height = static_cast<uint16_t>(dst_height)};
        VA_CALL(_va_vtable->vaCreateBuffer(_va_driver, _va_context_id, VAProcPipelineParameterBufferType,
                                           sizeof(pipeline_param_bufs[i]), 1, &pipeline_param_bufs[i],
                                           &pipeline_param_buf_ids[i]));
    }

    VAAPIBufferPtr dst_vaapi = dst_buffer_to_vaapi(dst);
    if (!dst_vaapi)
        throw std::runtime_error("Couldn't convert Buffer to VAAPIBuffer");
    VASurfaceID dst_surface = dst_vaapi->va_surface();

    VA_CALL(_va_vtable->vaBeginPicture(_va_driver, _va_context_id, dst_surface));
    VA_CALL(_va_vtable->vaRenderPicture(_va_driver, _va_context_id, &pipeline_param_buf_ids[0], _src_batch.size()));
    VA_CALL(_va_vtable->vaEndPicture(_va_driver, _va_context_id));

    for (size_t i = 0; i < _batch_size; i++) {
        VA_CALL(_va_vtable->vaDestroyBuffer(_va_driver, pipeline_param_buf_ids[i]));
    }

    // Attach to output buffer information about pts/stream_id/object_id of every source frame
    for (size_t i = 0; i < _batch_size; i++) {
        auto src_meta = find_metadata<SourceIdentifierMetadata>(*_src_batch[i]);
        if (src_meta) {
            auto dst_meta = dst->add_metadata(SourceIdentifierMetadata::name);
            copy_dictionary(*src_meta, *dst_meta);
            dst_meta->set(SourceIdentifierMetadata::key::batch_index, static_cast<int>(i));
        }
    }

    _src_batch.clear();

    return true;
}

BufferInfo VideoPreprocVAAPI::set_info_types(BufferInfo info, const BufferInfo &static_info) {
    info.media_type = static_info.media_type;
    info.buffer_type = static_info.buffer_type;
    info.format = static_info.format;
    return info;
}

VAAPIBufferPtr VideoPreprocVAAPI::dst_buffer_to_vaapi(BufferPtr dst) {
    return std::dynamic_pointer_cast<VAAPIBuffer>(dst);
}

TransformDesc VideoPreprocVAAPIDesc = {.name = "video_preproc_vaapi",
                                       .description = "Batched pre-processing with VAAPI memory as input and output",
                                       .author = "Intel Corporation",
                                       .params = VideoPreprocVAAPIParamsDesc(),
                                       .input_info = {{FourCC::FOURCC_BGRX, BufferType::VAAPI_SURFACE}},
                                       .output_info = {{MediaType::TENSORS, BufferType::VAAPI_SURFACE}},
                                       .create = TransformBase::create<VideoPreprocVAAPI>,
                                       .flags = TRANSFORM_FLAG_OUTPUT_ALLOCATOR | TRANSFORM_FLAG_SHARABLE |
                                                TRANSFORM_FLAG_MULTISTREAM_MUXER};

} // namespace dlstreamer
