/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <CL/sycl.hpp>

#include "dlstreamer/sycl/elements/sycl_meta_overlay.h"

#include "base_watermark.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/cpu/frame_alloc.h"
#include "dlstreamer/cpu/utils.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/sycl/context.h"
#include "dlstreamer/sycl/sycl_usm_tensor.h"
#include "dlstreamer/utils.h"
#include "dlstreamer/vaapi/context.h"

namespace dlstreamer {

class SyclMetaOverlay : public BaseWatermark {
  public:
    SyclMetaOverlay(DictionaryCPtr params, const ContextPtr &app_context) : BaseWatermark(params, app_context) {
        _queue = sycl::queue(sycl::gpu_selector()); // TODO handle device parameter
        _sycl_context = SYCLContext::create(_queue);
    }

    ~SyclMetaOverlay() {
    }

    bool init_once() override {
        auto vaapi_context = VAAPIContext::create(_app_context);
        auto dma_context = DMAContext::create(_app_context);
        create_mapper({_app_context, vaapi_context, dma_context, _sycl_context}, true);

        return true;
    }

    bool process(FramePtr frame) override {
        std::lock_guard<std::mutex> guard(_mutex);

        // All regions plus full frame
        std::vector<FramePtr> regions = frame->regions();
        regions.push_back(frame);
        size_t num_regions = regions.size();

        // Metadata to store SYCL pointers
        auto device_mem_meta = find_metadata(*frame, _device_mem_meta_name);
        if (!device_mem_meta)
            device_mem_meta = frame->metadata().add(_device_mem_meta_name);
        DLS_CHECK(device_mem_meta);

        // prepare data for all primitives
        std::vector<RectPrim> rects(num_regions);
        std::vector<MaskPrim> masks(num_regions);
        size_t num_rects = 0, num_texts = 0, num_masks = 0;
        prepare_prims(frame, regions, rects.data(), num_rects, NULL, num_texts, masks.data(), num_masks);

        // map frame and render
        auto tensor = frame->tensor().map(_sycl_context, AccessMode::Write);
        sycl::event e0, e1;
        if (num_rects)
            e0 = render_rectangles(tensor, rects.data(), num_rects);
        if (num_masks)
            e1 = render_masks(tensor, masks.data(), num_masks);

        // wait for all DPC++ kernels
        sycl::event::wait({e0, e1});
        _queue.wait(); // TODO

        // due to DPC++ bug, free device memory allocated by render_masks() with some delay
        auto sycl_context = _sycl_context;
        std::thread([masks, num_masks, sycl_context] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 100ms
            for (size_t i = 0; i < num_masks; i++)
                sycl_context->free(masks[i].data);
        }).detach();

        return true;
    }

  private:
    sycl::queue _queue;
    SYCLContextPtr _sycl_context;
    std::mutex _mutex;
    static constexpr auto _device_mem_meta_name = "device_mem_meta";

    sycl::event render_rectangles(const TensorPtr &tensor, RectPrim *rects, size_t num_rects) {
        uint32_t *data = reinterpret_cast<uint32_t *>(tensor->data<uint8_t>());
        ImageInfo info(tensor->info());
        DLS_CHECK(info.layout() == ImageLayout::HWC || info.layout() == ImageLayout::NHWC);
        DLS_CHECK(info.channels() == 4);
        size_t stride = info.width_stride() / sizeof(uint32_t);

        size_t max_length = 0;
        for (size_t i = 0; i < num_rects; i++) {
            const RectPrim &rect = rects[i];
            if (rect.height + 2 * rect.thickness > max_length)
                max_length = rect.height + 2 * rect.thickness;
            if (rect.width + 2 * rect.thickness > max_length)
                max_length = rect.width + 2 * rect.thickness;
        }

        size_t local_length = 0;
        size_t wgroup_size = _queue.get_device().get_info<sycl::info::device::max_work_group_size>();
        if (max_length <= wgroup_size) {
            local_length = max_length;
        } else {
            local_length = wgroup_size;
            max_length = (max_length / wgroup_size + 1) * wgroup_size;
        }

        sycl::range global{num_rects, max_length};
        sycl::range local{1, local_length};

        sycl::buffer<RectPrim, 1> sycl_rects(rects, sycl::range<1>(num_rects));
        return _queue.submit([&](sycl::handler &cgh) {
            auto rects_acc = sycl_rects.get_access<sycl::access::mode::read>(cgh);

            cgh.parallel_for<class RenderRectangle>(sycl::nd_range{global, local}, [=](sycl::nd_item<2> item) {
                const int k = item.get_global_id(0);
                const int i = item.get_global_id(1);
                const auto &rect = rects_acc[k];
                const uint32_t x = rect.x + i;
                const uint32_t y = rect.y + i;
                if (x <= rect.x + rect.width + rect.thickness) {
                    auto data0 = data + x + rect.y * stride;
                    auto data1 = data0 + (rect.height + rect.thickness) * stride;
                    for (uint32_t j = 0; j < rect.thickness; j++) {
                        *data0 = rect.color;
                        *data1 = rect.color;
                        data0 += stride;
                        data1 += stride;
                    }
                }
                if (y <= rect.y + rect.height + rect.thickness) {
                    auto data0 = data + rect.x + y * stride;
                    auto data1 = data0 + rect.width + rect.thickness;
                    for (uint32_t j = 0; j < rect.thickness; j++) {
                        *data0++ = rect.color;
                        *data1++ = rect.color;
                    }
                }
            });
        });
    }

    sycl::event render_masks(const TensorPtr &tensor, MaskPrim *masks, size_t num_masks) {
        uint32_t *data = reinterpret_cast<uint32_t *>(tensor->data<uint8_t>());
        ImageInfo info(tensor->info());
        DLS_CHECK(info.layout() == ImageLayout::HWC || info.layout() == ImageLayout::NHWC);
        DLS_CHECK(info.channels() == 4);
        size_t stride = info.width_stride() / sizeof(uint32_t);
        size_t width = info.width();
        size_t height = info.height();

        size_t max_width = 0;
        size_t max_height = 0;
        for (size_t i = 0; i < num_masks; i++) {
            MaskPrim &mask = masks[i];
            // max_width and max_height
            if (size_t(mask.w) > max_width)
                max_width = mask.w;
            if (size_t(mask.h) > max_height)
                max_height = mask.h;
#if 1
            // copy to device memory
            size_t mask_size = mask.w * mask.h;
            uint8_t *device_mem = _sycl_context->malloc<uint8_t>(mask_size, sycl::usm::alloc::device);
            _queue.memcpy(device_mem, mask.data, mask_size).wait();
            mask.data = device_mem;
#endif
        }

#if 0
        // masks data as 3D buffer
        sycl::buffer<uint8_t, 3> masks_data(sycl::range<3>(num_masks, max_height, max_width));
        {
            auto masks_data_acc = masks_data.get_access<sycl::access::mode::write>();
            for (size_t k = 0; k < num_masks; k++) {
                MaskPrim &mask = masks[k];
                // TODO optimize
                for (int y = 0; y < mask.h; y++)
                    for (int x = 0; x < mask.w; x++)
                        masks_data_acc[k][y][x] = mask.data[y * mask.w + x];
            }
        }
#endif

        size_t wgroup_size = _queue.get_device().get_info<sycl::info::device::max_work_group_size>();
        size_t local_width = 0;
        size_t local_height = 0;
        if (max_width <= wgroup_size) {
            local_width = max_width;
            wgroup_size = wgroup_size / local_width;
            if (max_height <= wgroup_size) {
                local_height = max_height;
            } else {
                local_height = wgroup_size;
                max_height = (max_height / wgroup_size + 1) * wgroup_size;
            }
        } else {
            local_width = wgroup_size;
            local_height = 1;
            max_width = (max_width / wgroup_size + 1) * wgroup_size;
        }

        sycl::range global{num_masks, max_height, max_width};
        sycl::range local{1, local_height, local_width};

        sycl::buffer<MaskPrim, 1> sycl_masks(masks, sycl::range<1>(num_masks));
        return _queue.submit([&](sycl::handler &cgh) {
            auto masks_acc = sycl_masks.get_access<sycl::access::mode::read>(cgh);
            // auto masks_data_acc = masks_data.get_access<sycl::access::mode::read>(cgh);

            cgh.parallel_for<class RenderText>(sycl::nd_range{global, local}, [=](sycl::nd_item<3> item) {
                const size_t k = item.get_global_id(0);
                const size_t i = item.get_global_id(1);
                const size_t j = item.get_global_id(2);
                const auto &mask = masks_acc[k];

                const size_t y = static_cast<size_t>(mask.y) + i;
                const size_t x = static_cast<size_t>(mask.x) + j;
                if (j < static_cast<size_t>(mask.w) && i < static_cast<size_t>(mask.h) && x < width && y < height) {
                    // if (masks_data_acc[k][i][j])
                    if (mask.data[j + i * mask.w])
                        data[x + y * stride] = mask.color;
                }
            });
        });
    }
};

extern "C" {
ElementDesc sycl_meta_overlay = {
    .name = "sycl_meta_overlay",
    .description = "Visualize inference results using DPC++/SYCL backend",
    .author = "Intel Corporation",
    .params = &SyclMetaOverlay::params_desc,
    .input_info = {FrameInfo(ImageFormat::BGRX, MemoryType::VAAPI), FrameInfo(ImageFormat::RGBX, MemoryType::VAAPI)},
    .output_info = {FrameInfo(ImageFormat::BGRX, MemoryType::VAAPI), FrameInfo(ImageFormat::RGBX, MemoryType::VAAPI)},
    .create = create_element<SyclMetaOverlay>,
    .flags = 0};
}

} // namespace dlstreamer
