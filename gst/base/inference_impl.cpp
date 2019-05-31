/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_impl.h"

#include "config.h"

#include "gst_allocator_wrapper.h"
#include "gva_buffer_map.h"
#include "gva_roi_meta.h"
#include "gva_tensor_meta.h"
#include "gva_utils.h"
#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "logger_functions.h"
#include "read_model_proc.h"

#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std::placeholders;
using namespace InferenceBackend;

namespace {

bool CheckObjectClass(std::string requested, GQuark quark) {
    if (requested.empty())
        return true;
    if (!quark)
        return false;
    return requested == g_quark_to_string(quark);
}

inline std::vector<std::string> SplitString(const std::string input, char delimiter = ',') {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(input);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

inline std::string GetStringArrayElem(const std::string &in_str, int index) {
    auto tokens = SplitString(in_str);
    if (index < 0 || (size_t)index >= tokens.size())
        return "";
    return tokens[index];
}

inline std::map<std::string, std::string> String2Map(std::string const &s) {
    std::string key, val;
    std::istringstream iss(s);
    std::map<std::string, std::string> m;

    while (std::getline(std::getline(iss, key, '=') >> std::ws, val)) {
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

inline std::map<std::string, std::string> CreateInferConfig(const char *const infer_config_str,
                                                            const char *const cpu_streams_c_str, const int nireq) {
    std::map<std::string, std::string> infer_config = String2Map(infer_config_str);
    if (cpu_streams_c_str != nullptr && strlen(cpu_streams_c_str) > 0) {
        std::string cpu_streams = cpu_streams_c_str;
        if (cpu_streams == "true")
            cpu_streams = std::to_string(nireq);
        if (cpu_streams != "false")
            infer_config[KEY_CPU_THROUGHPUT_STREAMS] = cpu_streams;
    }
    return infer_config;
}

} // namespace

InferenceImpl::ClassificationModel InferenceImpl::CreateClassificationModel(GvaBaseInference *gva_base_inference,
                                                                            std::shared_ptr<Allocator> &allocator,
                                                                            const std::string &model_file,
                                                                            const std::string &model_proc_path,
                                                                            const std::string &object_class) {

    GST_WARNING_OBJECT(gva_base_inference, "Loading model: device=%s, path=%s", gva_base_inference->device,
                       model_file.c_str());
    GST_WARNING_OBJECT(gva_base_inference, "Setting batch_size=%d, nireq=%d", gva_base_inference->batch_size,
                       gva_base_inference->nireq);
    set_log_function(GST_logger);
    std::map<std::string, std::string> infer_config =
        CreateInferConfig(gva_base_inference->infer_config, gva_base_inference->cpu_streams, gva_base_inference->nireq);

    auto infer = ImageInference::make_shared(MemoryType::ANY, gva_base_inference->device, model_file,
                                             gva_base_inference->batch_size, gva_base_inference->nireq, infer_config,
                                             allocator.get(),
                                             std::bind(&InferenceImpl::InferenceCompletionCallback, this, _1, _2));

    ClassificationModel model;
    model.inference = infer;
    model.model_name = infer->GetModelName();
    model.object_class = object_class;
    if (!model_proc_path.empty()) {
        model.model_proc = ReadModelProc(model_proc_path);
    }
    model.input_preproc = nullptr;
    for (auto proc : model.model_proc) {
        if (gst_structure_get_string(proc.second, "converter") && is_preprocessor(proc.second)) {
            model.input_preproc = proc.second;
            break;
        }
    }
    return model;
}

InferenceImpl::InferenceImpl(GvaBaseInference *gva_base_inference)
    : frame_num(-1), gva_base_inference(gva_base_inference) {
    if (!gva_base_inference->model) {
        throw std::runtime_error("Model not specified");
    }
    std::vector<std::string> model_files = SplitString(gva_base_inference->model);
    std::vector<std::string> model_procs;
    if (gva_base_inference->model_proc) {
        model_procs = SplitString(gva_base_inference->model_proc);
    }

    std::map<std::string, std::string> infer_config =
        CreateInferConfig(gva_base_inference->infer_config, gva_base_inference->cpu_streams, gva_base_inference->nireq);

    allocator = CreateAllocator(gva_base_inference->allocator_name);

    std::vector<std::string> object_classes = SplitString(gva_base_inference->object_class);
    for (size_t i = 0; i < model_files.size(); i++) {
        std::string model_proc = i < model_procs.size() ? model_procs[i] : std::string();
        std::string object_class = i < object_classes.size() ? object_classes[i] : "";
        auto model = CreateClassificationModel(gva_base_inference, allocator, model_files[i], model_proc, object_class);
        this->models.push_back(std::move(model));
    }
}

void InferenceImpl::FlushInference() {
    for (ClassificationModel &model : models) {
        model.inference->Flush();
    }
}

InferenceImpl::~InferenceImpl() {
    for (ClassificationModel &model : models) {
        for (auto proc : model.model_proc) {
            gst_structure_free(proc.second);
        }
    }
    models.clear();
}

void InferenceImpl::PushOutput() {
    while (!output_frames.empty()) {
        OutputFrame &front = output_frames.front();
        if (front.inference_count > 0) {
            break; // inference not completed yet
        }
        GstBuffer *buffer = front.writable_buffer ? front.writable_buffer : front.buffer;
        GstFlowReturn ret = gst_pad_push(GST_BASE_TRANSFORM_SRC_PAD(front.filter), buffer);
        if (ret != GST_FLOW_OK) {
            GST_WARNING("Inference gst_pad_push returned status %d", ret);
        }

        output_frames.pop_front();
    }
}

void InferenceImpl::SubmitImage(ClassificationModel &model, GstVideoRegionOfInterestMeta *meta,
                                InferenceBackend::Image &image, GstBuffer *buffer) {
    image.rect = {.x = (int)meta->x, .y = (int)meta->y, .width = (int)meta->w, .height = (int)meta->h};
    auto result = std::make_shared<InferenceResult>();
    result->inference_frame.buffer = buffer;
    result->inference_frame.roi = *meta;
    result->model = &model;

    std::function<void(InferenceBackend::Image &)> preProcessFunction = [](InferenceBackend::Image &) {};
    if (gva_base_inference->pre_proc && model.input_preproc) {
        preProcessFunction = [&](InferenceBackend::Image &image) {
            ((PreProcFunction)gva_base_inference->pre_proc)(model.input_preproc, image);
        };
    }
    if (gva_base_inference->get_roi_pre_proc && model.input_preproc) {
        preProcessFunction = ((GetROIPreProcFunction)gva_base_inference->get_roi_pre_proc)(model.input_preproc, meta);
    }
    model.inference->SubmitImage(image, result, preProcessFunction);
}

GstFlowReturn InferenceImpl::SubmitImages(const std::vector<GstVideoRegionOfInterestMeta *> &metas, GstVideoInfo *info,
                                          GstBuffer *buffer) {
    // return FLOW_DROPPED as we push buffers from separate thread
    GstFlowReturn return_status = GST_BASE_TRANSFORM_FLOW_DROPPED;

    InferenceBackend::Image image;
    BufferMapContext mapContext;

    gva_buffer_map(buffer, image, mapContext, info, InferenceBackend::MemoryType::ANY);
    try {
        for (ClassificationModel &model : models) {
            for (const auto meta : metas) {
                if (CheckObjectClass(model.object_class, meta->roi_type)) {
                    SubmitImage(model, meta, image, buffer);
                }
            }
        }
    } catch (const std::exception &e) {
        GST_ERROR("%s", CreateNestedErrorMsg(e).c_str());
        return_status = GST_FLOW_ERROR;
    }
    gva_buffer_unmap(buffer, image, mapContext);

    return return_status;
}

GstFlowReturn InferenceImpl::TransformFrameIp(GvaBaseInference *gva_base_inference, GstBaseTransform *trans,
                                              GstBuffer *buffer, GstVideoInfo *info) {
    std::unique_lock<std::mutex> lock(_mutex);

    // Collect all ROI metas into std::vector
    std::vector<GstVideoRegionOfInterestMeta *> metas;
    GstVideoRegionOfInterestMeta full_frame_meta;
    if (gva_base_inference->is_full_frame) {
        full_frame_meta = {};
        full_frame_meta.x = 0;
        full_frame_meta.y = 0;
        full_frame_meta.w = info->width;
        full_frame_meta.h = info->height;
        metas.push_back(&full_frame_meta);
    } else {
        GstVideoRegionOfInterestMeta *meta = NULL;
        gpointer state = NULL;
        while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
            metas.push_back(meta);
        }
    }

    frame_num++;
    // count number ROIs to run inference on
    int inference_count = 0;

    for (ClassificationModel &model : models) {
        for (auto meta : metas) {
            if (CheckObjectClass(model.object_class, meta->roi_type)) {
                inference_count++;
            }
        }
    }

    bool run_inference =
        !(inference_count == 0 ||
          // gva_base_inference->every_nth_frame == -1 || // TODO separate property instead of -1
          (gva_base_inference->every_nth_frame > 0 && frame_num % gva_base_inference->every_nth_frame > 0));

    // push into output_frames queue
    {
        std::lock_guard<std::mutex> guard(output_frames_mutex);
        if (!run_inference && output_frames.empty()) {
            return GST_FLOW_OK;
        }

        // increment buffer reference
        buffer = gst_buffer_ref(buffer);
        InferenceImpl::OutputFrame output_frame = {.buffer = buffer,
                                                   .writable_buffer = NULL,
                                                   .inference_count = run_inference ? inference_count : 0,
                                                   .filter = trans};
        output_frames.push_back(output_frame);

        if (!run_inference) {
            // If we don't need to run inference and there are no frames queued for inference then finish transform
            return GST_BASE_TRANSFORM_FLOW_DROPPED;
        }
    }

    return SubmitImages(metas, info, buffer);
}

void InferenceImpl::SinkEvent(GstEvent *event) {
    if (event->type == GST_EVENT_EOS) {
        for (ClassificationModel &model : models) {
            model.inference->Flush();
        }
    }
}

void InferenceImpl::InferenceCompletionCallback(
    std::map<std::string, InferenceBackend::OutputBlob::Ptr> blobs,
    std::vector<std::shared_ptr<InferenceBackend::ImageInference::IFrameBase>> frames) {
    if (frames.empty())
        return;

    std::lock_guard<std::mutex> guard(output_frames_mutex);

    std::vector<InferenceROI> inference_frames;
    ClassificationModel *model = nullptr;
    for (auto &frame : frames) {
        auto f = std::dynamic_pointer_cast<InferenceResult>(frame);
        model = f->model;
        inference_frames.push_back(f->inference_frame);
        GstBuffer *buffer = inference_frames.back().buffer;
        // check if we have writable version of this buffer (this function called multiple times on same buffer)
        for (OutputFrame &output : output_frames) {
            if (output.buffer == buffer) {
                if (output.writable_buffer)
                    buffer = inference_frames.back().buffer = output.writable_buffer;
                break;
            }
        }
        // if buffer not writable, make it writable as we need to attach metadata
        if (!gst_buffer_is_writable(buffer)) {
            GstBuffer *writable_buffer = gst_buffer_make_writable(buffer);
            inference_frames.back().buffer = writable_buffer;
            // replace buffer pointer in 'output_frames' queue
            for (OutputFrame &output_frame : output_frames) {
                if (output_frame.buffer == buffer) {
                    output_frame.writable_buffer = writable_buffer;
                    break;
                }
            }
        }
    }

    if (gva_base_inference->post_proc) {
        ((PostProcFunction)gva_base_inference->post_proc)(blobs, inference_frames, model->model_proc,
                                                          model ? model->model_name.c_str() : nullptr,
                                                          gva_base_inference);
    }

    for (InferenceROI &frame : inference_frames) {
        for (OutputFrame &output : output_frames) {
            if (frame.buffer == output.buffer || frame.buffer == output.writable_buffer) {
                output.inference_count--;
                break;
            }
        }
    }

    PushOutput();
}
