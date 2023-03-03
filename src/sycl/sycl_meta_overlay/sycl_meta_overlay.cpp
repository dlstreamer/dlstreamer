/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <CL/sycl.hpp>

#include "dlstreamer/sycl/elements/sycl_meta_overlay.h"

#include "base_meta_overlay.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/cpu/frame_alloc.h"
#include "dlstreamer/cpu/utils.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/sycl/context.h"
#include "dlstreamer/sycl/sycl_usm_tensor.h"
#include "dlstreamer/utils.h"
#include "dlstreamer/vaapi/context.h"
#include "dlstreamer_logger.h"

namespace dlstreamer {

class SyclMetaOverlay : public MetaOverlayBase {
  public:
    SyclMetaOverlay(DictionaryCPtr params, const ContextPtr &app_context) : MetaOverlayBase(params, app_context) {
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
        auto task = itt::Task(__FILE__ ":process");
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
        std::vector<overlay::prims::Rect> rects;
        std::vector<overlay::prims::Mask> masks;
        std::vector<overlay::prims::Circle> keypoints;
        std::vector<overlay::prims::Line> lines;
        rects.reserve(num_regions);
        masks.reserve(num_regions);
        prepare_prims(frame, regions, &rects, nullptr, &masks, &keypoints, &lines);

        // map frame and render
        auto tensor = frame->tensor().map(_sycl_context, AccessMode::Write);
        std::vector<sycl::event> events;
        events.reserve(5);
        if (!rects.empty())
            events.emplace_back(render_rectangles(tensor, rects.data(), rects.size()));
        if (!masks.empty())
            events.emplace_back(render_masks(tensor, masks.data(), masks.size()));
        if (!keypoints.empty())
            events.emplace_back(render_circles(tensor, keypoints.data(), keypoints.size()));
        if (!lines.empty()) {
            std::vector<overlay::prims::Line> lines_low;
            std::vector<overlay::prims::Line> lines_hi;
            lines_low.reserve(lines.size());
            lines_hi.reserve(lines.size());
            for (auto &line : lines) {
                prepare_line(line);
                if (line.steep)
                    lines_hi.emplace_back(line);
                else
                    lines_low.emplace_back(line);
            }
            if (!lines_hi.empty())
                events.emplace_back(render_lines_hi(tensor, lines_hi.data(), lines_hi.size()));
            if (!lines_low.empty())
                events.emplace_back(render_lines_low(tensor, lines_low.data(), lines_low.size()));
        }

        // wait for all DPC++ kernels
        sycl::event::wait(events);
        _queue.wait(); // TODO

        // due to DPC++ bug, free device memory allocated by render_masks() with some delay
        auto sycl_context = _sycl_context;
        std::thread([masks, sycl_context] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 100ms
            for (size_t i = 0; i < masks.size(); i++)
                sycl_context->free(masks[i].data);
        }).detach();

        return true;
    }

  private:
    sycl::queue _queue;
    SYCLContextPtr _sycl_context;
    std::mutex _mutex;
    static constexpr auto _device_mem_meta_name = "device_mem_meta";
    void prepare_line(overlay::prims::Line &l) {
        const int dx = l.x2 - l.x1;
        const int dy = l.y2 - l.y1;
        l.steep = abs(dy) > abs(dx);
        bool swap = false;
        if (l.steep)
            swap = dy < 0;
        else
            swap = dx < 0;

        if (swap) {
            std::swap(l.x1, l.x2);
            std::swap(l.y1, l.y2);
        }
    }
    sycl::event render_rectangles(const TensorPtr &tensor, overlay::prims::Rect *rects, size_t num_rects) {
        auto task = itt::Task(__FILE__ ":render_rectangles");
        uint32_t *data = reinterpret_cast<uint32_t *>(tensor->data<uint8_t>());
        ImageInfo info(tensor->info());
        DLS_CHECK(info.layout() == ImageLayout::HWC || info.layout() == ImageLayout::NHWC);
        DLS_CHECK(info.channels() == 4);
        size_t stride = info.width_stride() / sizeof(uint32_t);

        size_t max_length = 0;
        for (size_t i = 0; i < num_rects; i++) {
            const overlay::prims::Rect &rect = rects[i];
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
        auto task_buffer_allocation = itt::Task(__FILE__ ":render_rectangles:buffer_allocation");
        sycl::buffer<overlay::prims::Rect, 1> sycl_rects(rects, sycl::range<1>(num_rects));
        return _queue.submit([&](sycl::handler &cgh) {
            auto rects_acc = sycl_rects.get_access<sycl::access::mode::read>(cgh);
            task_buffer_allocation.end();

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

    sycl::event render_masks(const TensorPtr &tensor, overlay::prims::Mask *masks, size_t num_masks) {
        auto task = itt::Task(__FILE__ ":render_masks");
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
            overlay::prims::Mask &mask = masks[i];
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
                overlay::prims::Mask &mask = masks[k];
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

        sycl::buffer<overlay::prims::Mask, 1> sycl_masks(masks, sycl::range<1>(num_masks));
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

    sycl::event render_circles(const TensorPtr &tensor, overlay::prims::Circle *circles, size_t num_circles) {
        auto task = itt::Task(__FILE__ ":render_circles");
        uint32_t *data = reinterpret_cast<uint32_t *>(tensor->data<uint8_t>());
        ImageInfo info(tensor->info());
        DLS_CHECK(info.layout() == ImageLayout::HWC || info.layout() == ImageLayout::NHWC);
        DLS_CHECK(info.channels() == 4);
        size_t stride = info.width_stride() / sizeof(uint32_t);
        size_t max_radius = 0;
        for (size_t i = 0; i < num_circles; i++) {
            const overlay::prims::Circle &circle = circles[i];
            if (circle.radius > max_radius)
                max_radius = circle.radius;
        }
        auto max_d = max_radius * 2;
        size_t wgroup_size = _queue.get_device().get_info<sycl::info::device::max_work_group_size>();
        size_t local_width = 1;
        size_t local_height = 1;

        if (max_d <= wgroup_size) {
            local_width = max_d;
            wgroup_size = wgroup_size / local_width;
        } else {
            local_width = wgroup_size;
            max_d = (max_d / wgroup_size + 1) * wgroup_size;
        }

        sycl::range global{num_circles, max_d, max_d};
        sycl::range local{1, local_height, local_width};
        sycl::buffer<overlay::prims::Circle, 1> sycl_circles(circles, sycl::range<1>(num_circles));
        return _queue.submit([&](sycl::handler &cgh) {
            auto circle_acc = sycl_circles.get_access<sycl::access::mode::read>(cgh);

            cgh.parallel_for<class RenderCircle>(sycl::nd_range{global, local}, [=](sycl::nd_item<3> item) {
                const int k = item.get_global_id(0);
                const int i = item.get_global_id(1);
                const int j = item.get_global_id(2);

                const auto &circle = circle_acc[k];
                const int r2 = circle.radius * circle.radius + 1;

                const size_t y = circle.y - circle.radius + i;
                const size_t x = circle.x - circle.radius + j;
                const int dx = circle.x - x;
                const int dy = circle.y - y;
                if (x <= stride) {

                    if (dx * dx + dy * dy < r2) {
                        auto data0 = data + x + y * stride;
                        *data0 = circle.color;
                    }
                }
            });
        });
    }

    sycl::event render_lines_low(const TensorPtr &tensor, overlay::prims::Line *lines, size_t num_lines) {
        auto task = itt::Task(__FILE__ ":render_lines_low");
        auto thick = lines[0].thickness;
        sycl::range global{num_lines, thick};
        sycl::range local{1, 1};
        uint32_t *data = reinterpret_cast<uint32_t *>(tensor->data<uint8_t>());
        ImageInfo info(tensor->info());
        DLS_CHECK(info.layout() == ImageLayout::HWC || info.layout() == ImageLayout::NHWC);
        DLS_CHECK(info.channels() == 4);
        size_t stride = info.width_stride() / sizeof(uint32_t);

        sycl::buffer<overlay::prims::Line, 1> sycl_lines(lines, sycl::range<1>(num_lines));
        return _queue.submit([&](sycl::handler &cgh) {
            auto line_acc = sycl_lines.get_access<sycl::access::mode::read>(cgh);

            cgh.parallel_for<class RenderLineLow>(sycl::nd_range{global, local}, [=](sycl::nd_item<2> item) {
                const int k = item.get_global_id(0); // Index of line in array
                const int i = item.get_global_id(1);

                const auto &line = line_acc[k];
                // Thikness handling
                const auto y1 = line.y1 + i;
                const auto y2 = line.y2 + i;

                const int dx = abs(line.x2 - line.x1);
                const int dy = abs(y2 - y1);

                // y increment lookup table
                const int look_y[] = {0, (y1 < y2) ? 1 : -1};
                // Error lookup table
                const int look_err[] = {dy, dy - dx};
                // Initial error value
                int error = dy - dx / 2;

                int y = y1;
                for (int x = line.x1; x != line.x2 + 1; x++) {
                    auto data0 = data + x + y * stride;
                    *data0 = line.color;

                    const bool ec = error >= 0;
                    // Increment y and error values based on error check value
                    y += look_y[ec];
                    error += look_err[ec];
                }
            });
        });
    }

    sycl::event render_lines_hi(const TensorPtr &tensor, overlay::prims::Line *lines, size_t num_lines) {
        auto task = itt::Task(__FILE__ ":render_lines_hi");
        auto thick = lines[0].thickness;
        sycl::range global{num_lines, thick};
        sycl::range local{1, 1};
        uint32_t *data = reinterpret_cast<uint32_t *>(tensor->data<uint8_t>());
        ImageInfo info(tensor->info());
        DLS_CHECK(info.layout() == ImageLayout::HWC || info.layout() == ImageLayout::NHWC);
        DLS_CHECK(info.channels() == 4);
        size_t stride = info.width_stride() / sizeof(uint32_t);

        sycl::buffer<overlay::prims::Line, 1> sycl_lines(lines, sycl::range<1>(num_lines));
        return _queue.submit([&](sycl::handler &cgh) {
            auto line_acc = sycl_lines.get_access<sycl::access::mode::read>(cgh);

            cgh.parallel_for<class RenderLineHi>(sycl::nd_range{global, local}, [=](sycl::nd_item<2> item) {
                const int k = item.get_global_id(0); // Index of line in array
                const int i = item.get_global_id(1);

                const auto &line = line_acc[k];
                // Thikness handling
                const auto x1 = line.x1 + i;
                const auto x2 = line.x2 + i;

                const int dx = abs(x2 - x1);
                const int dy = abs(line.y2 - line.y1);

                // x increment lookup table
                const int look_x[] = {0, (x1 < x2) ? 1 : -1};
                // Error lookup table
                const int look_err[] = {dx, dx - dy};
                // Initial error value
                int error = dx - dy / 2;

                int x = x1;
                for (int y = line.y1; y != line.y2 + 1; y++) {
                    auto data0 = data + x + y * stride;
                    *data0 = line.color;

                    const bool ec = error >= 0;
                    // Increment y and error values based on error check value
                    x += look_x[ec];
                    error += look_err[ec];
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
