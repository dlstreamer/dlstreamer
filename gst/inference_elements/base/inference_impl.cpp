/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_impl.h"

#include "config.h"

#include "gst_allocator_wrapper.h"
#include "gva_buffer_map.h"
#include "gva_utils.h"
#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "logger_functions.h"
#include "read_model_proc.h"
#include "video_frame.h"

#include <assert.h>
#include <cstring>
#include <exception>
#include <gst/allocators/allocators.h>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std::placeholders;
using namespace InferenceBackend;

namespace {

inline std::map<std::string, std::string> StringToMap(std::string const &s, char records_delimiter = ',',
                                                      char key_val_delimiter = '=') {
    std::string key, val;
    std::istringstream iss(s);
    std::map<std::string, std::string> m;

    while (std::getline(std::getline(iss, key, key_val_delimiter) >> std::ws, val, records_delimiter)) {
        m[key] = val;
    }

    return m;
}

inline std::shared_ptr<Allocator> CreateAllocator(const char *const allocator_name) {
    std::shared_ptr<Allocator> allocator;
    if (allocator_name != nullptr) {
        try {
            allocator = std::make_shared<GstAllocatorWrapper>(allocator_name);
            GVA_TRACE("GstAllocatorWrapper is created");
        } catch (const std::exception &e) {
            GVA_ERROR(e.what());
            throw;
        }
    }
    return allocator;
}

inline std::string GstVideoFormatToString(GstVideoFormat formatType) {
    switch (formatType) {
    case GST_VIDEO_FORMAT_RGBA:
        return "RGBA";
    case GST_VIDEO_FORMAT_BGRA:
        return "BGRA";
    case GST_VIDEO_FORMAT_RGBx:
        return "RGBX";
    case GST_VIDEO_FORMAT_BGRx:
        return "BGRX";
    case GST_VIDEO_FORMAT_RGB:
        return "RGB";
    case GST_VIDEO_FORMAT_BGR:
        return "BGR";
    case GST_VIDEO_FORMAT_NV12:
        return "NV12";
    case GST_VIDEO_FORMAT_I420:
        return "I420";
    default:
        return "";
    }
}

void ParseExtension(const char *extension_string, std::map<std::string, std::string> &base_config) {
    // gva_base_inference->extension is device1=extension1,device2=extension2 like string
    std::map<std::string, std::string> extensions = StringToMap(extension_string);
    static std::map<std::string, std::string> supported_extensions = {
        {"CPU", KEY_CPU_EXTENSION}, {"GPU", KEY_GPU_EXTENSION}, {"VPU", KEY_VPU_EXTENSION}};

    for (auto it = extensions.begin(); it != extensions.end(); ++it) {
        auto it_ = supported_extensions.find(it->first);
        if (it_ != supported_extensions.end()) {
            base_config[it_->second] = it->second;
            extensions.erase(it->first);
        } else {
            std::runtime_error("Does not support extension for device " + it->first);
        }
    }
}

inline std::map<std::string, std::map<std::string, std::string>>
CreateNestedConfig(const GvaBaseInference *gva_base_inference) {
    std::map<std::string, std::map<std::string, std::string>> config;
    std::map<std::string, std::string> base;
    std::map<std::string, std::string> inference = StringToMap(gva_base_inference->infer_config);

    base[KEY_NIREQ] = std::to_string(gva_base_inference->nireq);
    if (gva_base_inference->device != nullptr && strlen(gva_base_inference->device) > 0) {
        std::string device = gva_base_inference->device;
        base[KEY_DEVICE] = device;
        if (device == "CPU") {
            if (inference.find(KEY_CPU_THROUGHPUT_STREAMS) == inference.end()) {
                inference[KEY_CPU_THROUGHPUT_STREAMS] = (gva_base_inference->cpu_streams == 0)
                                                            ? "CPU_THROUGHPUT_AUTO"
                                                            : std::to_string(gva_base_inference->cpu_streams);
            }
        }
        if (device == "GPU") {
            if (inference.find(KEY_GPU_THROUGHPUT_STREAMS) == inference.end()) {
                inference[KEY_GPU_THROUGHPUT_STREAMS] = (gva_base_inference->gpu_streams == 0)
                                                            ? "GPU_THROUGHPUT_AUTO"
                                                            : std::to_string(gva_base_inference->gpu_streams);
            }
        }
    }
    if (gva_base_inference->pre_proc_name != nullptr) {
        base[KEY_PRE_PROCESSOR_TYPE] = std::string(gva_base_inference->pre_proc_name);
    }
    base[KEY_IMAGE_FORMAT] =
        GstVideoFormatToString(static_cast<GstVideoFormat>(gva_base_inference->info->finfo->format));

    base[KEY_RESHAPE] = std::to_string(gva_base_inference->reshape);
    base[KEY_BATCH_SIZE] = std::to_string(gva_base_inference->batch_size);
    if (gva_base_inference->reshape) {
        if ((gva_base_inference->reshape_width) || (gva_base_inference->reshape_height) ||
            (gva_base_inference->batch_size > 1)) {
            base[KEY_RESHAPE_WIDTH] = std::to_string(gva_base_inference->reshape_width);
            base[KEY_RESHAPE_HEIGHT] = std::to_string(gva_base_inference->reshape_height);
        } else {
            base[KEY_RESHAPE_WIDTH] = std::to_string(gva_base_inference->info->width);
            base[KEY_RESHAPE_HEIGHT] = std::to_string(gva_base_inference->info->height);
        }
    }

    ParseExtension(gva_base_inference->extension, base);

    config[KEY_BASE] = base;
    config[KEY_INFERENCE] = inference;

    return config;
}

void ApplyImageBoundaries(std::shared_ptr<InferenceBackend::Image> &image, const GstVideoRegionOfInterestMeta *meta) {
    // workaround for cases when tinyyolov2 output blob parsing result coordinates are out of image
    // boundaries
    image->rect = {.x = (int)meta->x,
                   .y = (int)meta->y,
                   .width = ((int)meta->x + (int)meta->w > image->width) ? (int)(image->width - meta->x) : (int)meta->w,
                   .height =
                       ((int)meta->y + (int)meta->h > image->height) ? (int)(image->height - meta->y) : (int)meta->h};
}

std::shared_ptr<InferenceBackend::Image> CreateImage(GstBuffer *buffer, GstVideoInfo *info,
                                                     InferenceBackend::MemoryType mem_type, GstMapFlags map_flags) {
    ITT_TASK(__FUNCTION__);
    std::unique_ptr<InferenceBackend::Image> unique_image = std::unique_ptr<InferenceBackend::Image>(new Image);
    std::shared_ptr<BufferMapContext> map_context = std::make_shared<BufferMapContext>();
    if (!map_context) {
        GST_ERROR("Could not create buffer map_context");
        return nullptr;
    }

    if (!gva_buffer_map(buffer, *unique_image, *map_context, info, mem_type, map_flags)) {
        GST_ERROR("Buffer mapping failed");
        return nullptr;
    }
    auto image_deleter = [buffer, map_context](InferenceBackend::Image *image) {
        gva_buffer_unmap(buffer, *image, *map_context);
        delete image;
    };
    return std::shared_ptr<InferenceBackend::Image>(unique_image.release(), image_deleter);
}

} // namespace

InferenceImpl::Model InferenceImpl::CreateModel(GvaBaseInference *gva_base_inference,
                                                std::shared_ptr<Allocator> &allocator, const std::string &model_file,
                                                const std::string &model_proc_path) {

    if (!file_exists(model_file)) {
        const std::string error_message = std::string("Model file does not exist (") + model_file + std::string(")");
        GST_ERROR_OBJECT(gva_base_inference, "%s", error_message.c_str());
        throw std::invalid_argument(error_message);
    }
    GST_WARNING_OBJECT(gva_base_inference, "Loading model: device=%s, path=%s", gva_base_inference->device,
                       model_file.c_str());
    GST_WARNING_OBJECT(gva_base_inference, "Initial settings batch_size=%d, nireq=%d", gva_base_inference->batch_size,
                       gva_base_inference->nireq);
    set_log_function(GST_logger);
    std::map<std::string, std::map<std::string, std::string>> infer_config = CreateNestedConfig(gva_base_inference);

    auto infer = ImageInference::make_shared(MemoryType::ANY, model_file, infer_config, allocator.get(),
                                             std::bind(&InferenceImpl::InferenceCompletionCallback, this, _1, _2));

    Model model;
    model.inference = infer;
    model.name = infer->GetModelName();
    if (!model_proc_path.empty()) {
        model.proc = ReadModelProc(model_proc_path);
    }
    model.input_preproc = nullptr;
    for (auto proc : model.proc) {
        if (gst_structure_get_string(proc.second, "converter") && is_preprocessor(proc.second)) {
            model.input_preproc = proc.second;
            break;
        }
    }
    return model;
}

InferenceImpl::InferenceImpl(GvaBaseInference *gva_base_inference) : frame_num(0) {
    assert(gva_base_inference != nullptr);

    if (!gva_base_inference->model) {
        throw std::runtime_error("Model not specified");
    }
    std::vector<std::string> model_files = SplitString(gva_base_inference->model);
    std::vector<std::string> model_procs;
    if (gva_base_inference->model_proc) {
        model_procs = SplitString(gva_base_inference->model_proc);
    }

    allocator = CreateAllocator(gva_base_inference->allocator_name);

    for (size_t i = 0; i < model_files.size(); i++) {
        std::string model_proc = i < model_procs.size() ? model_procs[i] : std::string();
        Model model = CreateModel(gva_base_inference, allocator, model_files[i], model_proc);
        this->models.push_back(std::move(model));
    }
}

void InferenceImpl::FlushInference() {
    for (Model &model : models) {
        model.inference->Flush();
    }
}

InferenceImpl::~InferenceImpl() {
    for (Model &model : models) {
        for (auto proc : model.proc) {
            gst_structure_free(proc.second);
        }
    }
    models.clear();
}

void InferenceImpl::PushOutput() {
    ITT_TASK(__FUNCTION__);
    while (!output_frames.empty()) {
        OutputFrame &front = output_frames.front();
        assert(front.inference_count >= 0);
        if (front.inference_count != 0) {
            break; // inference not completed yet
        }
        GstBuffer *buffer = front.writable_buffer ? front.writable_buffer : front.buffer;

        if (!check_gva_base_inference_stopped(front.filter)) {
            GstFlowReturn ret = gst_pad_push(GST_BASE_TRANSFORM_SRC_PAD(front.filter), buffer);
            if (ret != GST_FLOW_OK) {
                GST_WARNING("Inference gst_pad_push returned status %d", ret);
            }
        }
        output_frames.pop_front();
    }
}

std::shared_ptr<InferenceImpl::InferenceResult>
InferenceImpl::MakeInferenceResult(GvaBaseInference *gva_base_inference, Model &model,
                                   GstVideoRegionOfInterestMeta *meta, std::shared_ptr<InferenceBackend::Image> image,
                                   GstBuffer *buffer) {
    auto result = std::make_shared<InferenceResult>();

    result->inference_frame.buffer = buffer;
    result->inference_frame.roi = *meta;
    result->inference_frame.gva_base_inference = gva_base_inference;
    if (gva_base_inference->info)
        result->inference_frame.info = gst_video_info_copy(gva_base_inference->info);

    result->model = &model;
    result->image = image;
    return result;
}

RoiPreProcessorFunction InferenceImpl::GetPreProcFunction(GvaBaseInference *gva_base_inference,
                                                          GstStructure *input_preproc,
                                                          GstVideoRegionOfInterestMeta *meta) {
    RoiPreProcessorFunction pre_proc = [](InferenceBackend::Image &) {};
    if (input_preproc) {
        if (gva_base_inference->get_roi_pre_proc) {
            pre_proc = gva_base_inference->get_roi_pre_proc(input_preproc, meta);
        } else if (gva_base_inference->pre_proc) {
            pre_proc = [gva_base_inference, input_preproc](InferenceBackend::Image &image) {
                gva_base_inference->pre_proc(input_preproc, image);
            };
        }
    }
    return pre_proc;
}

GstFlowReturn InferenceImpl::SubmitImages(GvaBaseInference *gva_base_inference,
                                          const std::vector<GstVideoRegionOfInterestMeta *> &metas, GstVideoInfo *info,
                                          GstBuffer *buffer) {
    ITT_TASK(__FUNCTION__);
    // return FLOW_DROPPED as we push buffers from separate thread
    GstFlowReturn return_status = GST_BASE_TRANSFORM_FLOW_DROPPED;

    InferenceBackend::MemoryType mem_type = InferenceBackend::MemoryType::SYSTEM;
    if (std::string(gva_base_inference->pre_proc_name) == "vaapi") {
#ifdef HAVE_VAAPI
        mem_type = MemoryType::VAAPI;
#elif defined(SUPPORT_DMA_BUFFER)
        GstMemory *mem = gst_buffer_get_memory(buffer, 0);
        mem_type = gst_is_fd_memory(mem) ? MemoryType::DMA_BUFFER : MemoryType::SYSTEM;
        gst_memory_unref(mem);
#endif
    }
    try {
        std::shared_ptr<InferenceBackend::Image> image = CreateImage(buffer, info, mem_type, GST_MAP_READ);
        if (!image) {
            return_status = GST_FLOW_ERROR;
            goto EXIT_LABEL;
        }

        for (Model &model : models) {
            for (const auto meta : metas) {
                ApplyImageBoundaries(image, meta);
                auto result = MakeInferenceResult(gva_base_inference, model, meta, image, buffer);
                RoiPreProcessorFunction pre_proc = GetPreProcFunction(gva_base_inference, model.input_preproc, meta);
                model.inference->SubmitImage(*image, result, pre_proc);
            }
        }
    } catch (const std::exception &e) {
        GST_ERROR("%s", CreateNestedErrorMsg(e).c_str());
        return_status = GST_FLOW_ERROR;
    }

EXIT_LABEL:
    return return_status;
}

const std::vector<InferenceImpl::Model> &InferenceImpl::GetModels() const {
    return models;
}

GstFlowReturn InferenceImpl::TransformFrameIp(GvaBaseInference *gva_base_inference, GstBuffer *buffer,
                                              GstVideoInfo *info) {
    ITT_TASK(__FUNCTION__);
    std::unique_lock<std::mutex> lock(_mutex);

    assert(gva_base_inference != nullptr);

    InferenceStatus status = INFERENCE_EXECUTED;
    {
        ITT_TASK("InferenceImpl::TransformFrameIp check_skip");
        if (++gva_base_inference->num_skipped_frames < gva_base_inference->every_nth_frame) {
            status = INFERENCE_SKIPPED_PER_PROPERTY;
        }
        if (gva_base_inference->adaptive_skip) {
            for (auto model : models) {
                if (model.inference->IsQueueFull()) {
                    status = INFERENCE_SKIPPED_ADAPTIVE;
                    break;
                }
            }
        }
        if (status == INFERENCE_EXECUTED) {
            gva_base_inference->num_skipped_frames = 0;
        }
    }

    // Collect all ROI metas into std::vector
    std::vector<GstVideoRegionOfInterestMeta *> metas;
    GstVideoRegionOfInterestMeta full_frame_meta;
    {
        ITT_TASK("InferenceImpl::TransformFrameIp collect_meta");
        if (gva_base_inference->is_full_frame) {
            full_frame_meta = GstVideoRegionOfInterestMeta();
            full_frame_meta.x = 0;
            full_frame_meta.y = 0;
            full_frame_meta.w = info->width;
            full_frame_meta.h = info->height;
            metas.push_back(&full_frame_meta);
        } else {
            GVA::VideoFrame video_frame(buffer, info);
            for (GVA::RegionOfInterest &region : video_frame.regions()) {
                if (!gva_base_inference->is_roi_classification_needed ||
                    gva_base_inference->is_roi_classification_needed(gva_base_inference, frame_num, buffer,
                                                                     region.meta())) {
                    metas.push_back(region.meta());
                }
            }
        }
    }

    // count number ROIs to run inference on
    int inference_count = (status == INFERENCE_EXECUTED) ? metas.size() * models.size() : 0;
    frame_num++;

    // push into output_frames queue
    {
        ITT_TASK("InferenceImpl::TransformFrameIp pushIntoOutputFramesQueue");
        std::lock_guard<std::mutex> guard(output_frames_mutex);
        if (!inference_count && output_frames.empty()) {
            // If we don't need to run inference and there are no frames queued for inference then finish transform
            return GST_FLOW_OK;
        }

        // increment buffer reference
        buffer = gst_buffer_ref(buffer);
        InferenceImpl::OutputFrame output_frame = {.buffer = buffer,
                                                   .writable_buffer = NULL,
                                                   .inference_count = inference_count,
                                                   .filter = gva_base_inference};
        output_frames.push_back(output_frame);

        if (!inference_count) {
            return GST_BASE_TRANSFORM_FLOW_DROPPED;
        }
    }

    return SubmitImages(gva_base_inference, metas, info, buffer);
}

void InferenceImpl::SinkEvent(GstEvent *event) {
    if (event->type == GST_EVENT_EOS) {
        for (Model &model : models) {
            model.inference->Flush();
        }
    }
}

void InferenceImpl::InferenceCompletionCallback(
    std::map<std::string, InferenceBackend::OutputBlob::Ptr> blobs,
    std::vector<std::shared_ptr<InferenceBackend::ImageInference::IFrameBase>> frames) {
    std::lock_guard<std::mutex> guard(output_frames_mutex);
    ITT_TASK(__FUNCTION__);
    if (frames.empty())
        return;

    std::vector<InferenceFrame> inference_frames;
    Model *model = nullptr;
    PostProcFunction post_proc = nullptr;

    for (auto &frame : frames) {
        auto inference_result = std::dynamic_pointer_cast<InferenceResult>(frame);
        model = inference_result->model;
        InferenceFrame inference_roi = inference_result->inference_frame;
        inference_result->image = nullptr; // if image_deleter set, call image_deleter including gst_buffer_unref
                                           // before gst_buffer_make_writable

        if (post_proc == nullptr)
            post_proc = inference_roi.gva_base_inference->post_proc;
        else
            assert(post_proc == inference_roi.gva_base_inference->post_proc);

        for (OutputFrame &output_frame : output_frames) {
            if (output_frame.buffer == inference_roi.buffer) {
                if (output_frame.filter->is_full_frame) { // except gvaclassify because it doesn't attach new metadata
                    if (output_frame.inference_count == 0)
                        // This condition is necessary if two items in output_frames refer to the same buffer.
                        // If current output_frame.inference_count equals 0, then inference for this output_frame
                        // already happened, but buffer wasn't pushed further by pipeline yet. We skip this buffer
                        // to find another, to which current inference callback really belongs
                        continue;
                    if (output_frame.writable_buffer) {
                        // check if we have writable version of this buffer (this function called multiple times
                        // on same buffer)
                        inference_roi.buffer = output_frame.writable_buffer;
                    } else {
                        if (!gst_buffer_is_writable(inference_roi.buffer)) {
                            GST_WARNING_OBJECT(output_frame.filter, "Making a writable buffer requires buffer copy");
                        }
                        inference_roi.buffer = gst_buffer_make_writable(inference_roi.buffer);
                        output_frame.writable_buffer = inference_roi.buffer;
                    }
                }
                --output_frame.inference_count;
                break;
            }
        }
        inference_frames.push_back(inference_roi);
    }

    if (post_proc != nullptr) {
        post_proc(blobs, inference_frames, model->proc, model ? model->name.c_str() : nullptr);
    }

    PushOutput();
}
