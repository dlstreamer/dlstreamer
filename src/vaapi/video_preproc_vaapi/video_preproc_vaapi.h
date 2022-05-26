/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/buffer_mappers/dma_to_vaapi.h"
#include "dlstreamer/buffer_mappers/mapper_chain.h"
#include "dlstreamer/buffer_mappers/opencl_to_cpu.h"
#include "dlstreamer/buffer_mappers/opencl_to_dma.h"
#include "dlstreamer/gst/source_id.h"
#include "dlstreamer/metadata.h"
#include "dlstreamer/transform.h"
#include <climits>
#include <mutex>
#include <va/va_backend.h>

namespace dlstreamer {

extern dlstreamer::TransformDesc VideoPreprocVAAPIDesc;

extern ParamDescVector *VideoPreprocVAAPIParamsDesc();

class VideoPreprocVAAPI : public TransformWithAlloc {
  public:
    VideoPreprocVAAPI(ITransformController &transform_ctrl, DictionaryCPtr params);

    BufferInfoVector get_input_info(const BufferInfo &output_info) override;
    BufferInfoVector get_output_info(const BufferInfo &input_info) override;
    void set_info(const BufferInfo &input_info, const BufferInfo &output_info) override;
    bool process(BufferPtr src, BufferPtr dst) override;
    std::function<BufferPtr()> get_output_allocator() override;

    ContextPtr get_context(const std::string & /*name*/) override {
        return nullptr;
    }
    BufferMapperPtr get_output_mapper() override;

  protected:
    void init_vaapi();
    BufferInfo set_info_types(BufferInfo info, const BufferInfo &static_info);
    virtual VAAPIBufferPtr dst_buffer_to_vaapi(BufferPtr dst);

    BufferInfo _input_info;
    BufferInfo _output_info;
    std::vector<BufferPtr> _src_batch;
    size_t _batch_size = 0;
    BufferMapperPtr _input_mapper;
    std::mutex _mutex;
    TransformDesc *_desc = &VideoPreprocVAAPIDesc;
    static constexpr int RT_FORMAT = VA_RT_FORMAT_YUV420;

    VAAPIContextPtr _vaapi_context;
    VADriverContextP _va_driver = nullptr;
    VADriverVTable *_va_vtable = nullptr;
    VAConfigID _va_config_id = VA_INVALID_ID;
    VAContextID _va_context_id = VA_INVALID_ID;
};

} // namespace dlstreamer
