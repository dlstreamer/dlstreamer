/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "classifyinference.h"

#include <assert.h>
#include <gst/allocators/gstdmabuf.h>
#include <memory>
#include <thread>

#include "config.h"

#include "gva_buffer_map.h"
#include "gva_roi_meta.h"
#include "gva_tensor_meta.h"
#include "gva_utils.h"
#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"

#include "align_transform.h"
#include "gstgvaclassify.h"
#include "logger_functions.h"
#include "read_model_proc.h"

using namespace std::placeholders;

bool CheckObjectClass(std::string requested, GQuark quark) {
    if (requested.empty())
        return true;
    if (!quark)
        return false;
    return requested == g_quark_to_string(quark);
}

std::map<std::string, InferenceRefs *> ClassifyInference::inference_pool_;
std::mutex ClassifyInference::inference_pool_mutex_;

ClassifyInferenceProxy *ClassifyInference::aquire_instance(GstGvaClassify *ovino) {
    std::lock_guard<std::mutex> guard(inference_pool_mutex_);
    std::string name(ovino->inference_id);

    InferenceRefs *infRefs = nullptr;
    auto it = inference_pool_.find(name);
    if (it == inference_pool_.end()) {
        infRefs = new InferenceRefs;
        infRefs->numRefs++;
        infRefs->proxy = new ClassifyInferenceProxy;
        if (ovino->model) {
            infRefs->proxy->instance = new ClassifyInference(ovino);

            // save master element for later reference from slave elements
            infRefs->masterElement = ovino;
        } else {
            // lazy initialization
            infRefs->proxy->instance = nullptr;
            infRefs->elementsToInit.push_back(ovino);
        }
        inference_pool_.insert({name, infRefs}); // CHECKME will pair has ref to string or string copy?
    } else {
        infRefs = it->second;

        // always increment ref counter
        infRefs->numRefs++;

        // if model arg is passed and there is no instance yet
        // , we will create it and make available to all element instances
        if (!infRefs->proxy->instance && ovino->model) {
            infRefs->proxy->instance = new ClassifyInference(ovino);

            // save master element for later reference from slave elements
            infRefs->masterElement = ovino;

            // iterate throught already created elements and set their props
            ClassifyInference::initExistingElements(infRefs);
            infRefs->elementsToInit.clear();

        } else if (infRefs->proxy->instance && !ovino->model) {
            // TODO add warning saying that specified props will be ignored if specified for slave.
            // Setting missing props to current elem
            ClassifyInference::fillElementProps(ovino, infRefs->masterElement);
        } else if (infRefs->proxy->instance && ovino->model) {
            GST_WARNING_OBJECT(ovino, "Only one element for each inference-id can have other properties specified.");
            // TODO implement comprehensive validation if all options match btw slave and master and if not:
            // throw std::runtime_error("Only one element for each inference-id can have other properties specified.");
        }
    }
    return infRefs->proxy;
}

void ClassifyInference::release_instance(GstGvaClassify *ovino) {
    std::lock_guard<std::mutex> guard(inference_pool_mutex_);
    std::string name(ovino->inference_id);

    auto it = inference_pool_.find(name);
    if (it == inference_pool_.end())
        return;

    InferenceRefs *infRefs = it->second;
    auto refcounter = --infRefs->numRefs;
    if (refcounter == 0) {
        delete infRefs->proxy->instance;
        delete infRefs->proxy;
        delete infRefs;
        inference_pool_.erase(name);
    }
}

void ClassifyInference::fillElementProps(GstGvaClassify *targetElem, GstGvaClassify *masterElem) {
    assert(masterElem);
    g_free(targetElem->model);
    targetElem->model = g_strdup(masterElem->model);
    g_free(targetElem->object_class);
    targetElem->object_class = g_strdup(masterElem->object_class);
    g_free(targetElem->device);
    targetElem->device = g_strdup(masterElem->device);
    g_free(targetElem->model_proc);
    targetElem->model_proc = g_strdup(masterElem->model_proc);
    targetElem->batch_size = masterElem->batch_size;
    targetElem->every_nth_frame = masterElem->every_nth_frame;
    targetElem->nireq = masterElem->nireq;
    g_free(targetElem->cpu_extension);
    targetElem->cpu_extension = g_strdup(masterElem->cpu_extension);
    g_free(targetElem->gpu_extension);
    targetElem->gpu_extension = g_strdup(masterElem->gpu_extension);
    // no need to copy inference_id because it should match already.
}

void ClassifyInference::initExistingElements(InferenceRefs *infRefs) {
    assert(infRefs->masterElement);
    for (auto elem : infRefs->elementsToInit) {
        ClassifyInference::fillElementProps(elem, infRefs->masterElement);
    }
}

ClassifyInferenceProxy *aquire_classify_inference(GstGvaClassify *ovino, GError **error) {
    ClassifyInferenceProxy *proxy = nullptr;
    try {
        proxy = ClassifyInference::aquire_instance(ovino);
    } catch (const std::exception &exc) {
        g_set_error(error, 1, 1, "%s", exc.what());
    }
    return proxy;
}

void release_classify_inference(GstGvaClassify *ovino) {
    ClassifyInference::release_instance(ovino);
}

void classify_inference_sink_event(GstGvaClassify *ovino, GstEvent *event) {
    ovino->inference->instance->SinkEvent(event);
}

GstFlowReturn frame_to_classify_inference(GstGvaClassify *ovino, GstBaseTransform *trans, GstBuffer *buf,
                                          GstVideoInfo *info) {
    if (!ovino || !ovino->inference || !ovino->inference->instance) {
        GST_ERROR_OBJECT(ovino, "empty inference instance!!!!");
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }

    return ovino->inference->instance->TransformFrameIp(ovino, trans, buf, info);
}

void flush_inference_classify(GstGvaClassify *ovino) {
    if (!ovino || !ovino->inference || !ovino->inference->instance) {
        GST_ERROR_OBJECT(ovino, "empty inference instance!!!!");
        return;
    }

    ovino->inference->instance->FlushInference();
}

ClassifyInference::ClassifyInference(GstGvaClassify *ovino) : frame_num(-1), inference_id(ovino->inference_id) {
    std::vector<std::string> model_files;
    std::vector<std::string> model_proc;

    if (!ovino->model)
        throw std::runtime_error("Model not specified");
    model_files = SplitString(ovino->model);
    if (ovino->model_proc) {
        model_proc = SplitString(ovino->model_proc);
    }

    std::map<std::string, std::string> infer_config;
    if (ovino->cpu_extension && *ovino->cpu_extension)
        infer_config[InferenceBackend::KEY_CPU_EXTENSION] = ovino->cpu_extension;
    if (ovino->cpu_streams && *ovino->cpu_streams) {
        std::string cpu_streams = ovino->cpu_streams;
        if (cpu_streams == "true")
            cpu_streams = std::to_string(ovino->nireq);
        if (cpu_streams != "false")
            infer_config[InferenceBackend::KEY_CPU_THROUGHPUT_STREAMS] = cpu_streams;
    }

    for (size_t i = 0; i < model_files.size(); i++) {
        std::string &model_file = model_files[i];
        GST_WARNING_OBJECT(ovino, "Loading model: device=%s, path=%s", ovino->device, model_file.c_str());
        GST_WARNING_OBJECT(ovino, "Setting batch_size=%d, nireq=%d", ovino->batch_size, ovino->nireq);
        set_log_function(GST_logger);
        auto infer = InferenceBackend::ImageInference::make_shared(
            InferenceBackend::MemoryType::ANY, ovino->device, model_file, ovino->batch_size, ovino->nireq, infer_config,
            std::bind(&ClassifyInference::InferenceCompletionCallback, this, _1, _2));

        ClassificationModel model;
        model.inference = infer;
        model.model_name = infer->GetModelName();
        model.object_class = GetStringArrayElem(ovino->object_class, i);
        if (i < model_proc.size() && !model_proc[i].empty()) {
            model.model_proc = ReadModelProc(model_proc[i]);
        }
        model.input_preproc = nullptr;
        for (auto proc : model.model_proc) {
            if (gst_structure_get_string(proc.second, "converter") && is_preprocessor(proc.second)) {
                model.input_preproc = proc.second;
                break;
            }
        }
        this->models.push_back(std::move(model));
    }
}

void ClassifyInference::FlushInference() {
    for (ClassificationModel &model : models) {
        model.inference->Flush();
    }
}

ClassifyInference::~ClassifyInference() {
    for (ClassificationModel &model : models) {
        for (auto proc : model.model_proc) {
            gst_structure_free(proc.second);
        }
    }
    models.clear();
}

void ClassifyInference::PushOutput() {
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

GstFlowReturn ClassifyInference::TransformFrameIp(GstGvaClassify *ovino, GstBaseTransform *trans, GstBuffer *buffer,
                                                  GstVideoInfo *info) {
    std::unique_lock<std::mutex> lock(_mutex);

    // Collect all ROI metas into std::vector
    std::vector<GstVideoRegionOfInterestMeta *> metas;
    GstVideoRegionOfInterestMeta *meta = NULL;
    gpointer state = NULL;
    while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
        metas.push_back(meta);
    }

    bool bRunInference = true;
    frame_num++;

    if (metas.empty() || // ovino->every_nth_frame == -1 || // TODO separate property instead of -1
        (ovino->every_nth_frame > 0 && frame_num % ovino->every_nth_frame > 0)) {
        bRunInference = false;
    }

    // count number ROIs to run inference on
    int inference_count = 0;

    for (ClassificationModel &model : models) {
        for (auto meta : metas) {
            if (CheckObjectClass(model.object_class, meta->roi_type)) {
                inference_count++;
            }
        }
    }

    if (inference_count == 0) {
        bRunInference = false;
    }

    if (!bRunInference) {
        // If we don't need to run inference and there are no frames queued for inference then finish transform
        std::lock_guard<std::mutex> guard(output_frames_mutex);
        if (output_frames.empty()) {
            return GST_FLOW_OK;
        } else {
            output_frames.push_back(
                {.buffer = gst_buffer_ref(buffer), .writable_buffer = 0, .inference_count = 0, .filter = trans});
            return GST_BASE_TRANSFORM_FLOW_DROPPED;
        }
    }

    // increment buffer reference
    buffer = gst_buffer_ref(buffer);

    // push into output_frames queue
    {
        std::lock_guard<std::mutex> guard(output_frames_mutex);
        output_frames.push_back(
            {.buffer = buffer, .writable_buffer = NULL, .inference_count = inference_count, .filter = trans});
    }

    InferenceBackend::Image image;
    BufferMapContext mapContext;
    gva_buffer_map(buffer, image, mapContext, info, InferenceBackend::MemoryType::ANY);

    for (ClassificationModel &model : models) {
        for (auto meta : metas) {
            if (!CheckObjectClass(model.object_class, meta->roi_type)) {
                continue;
            }

            image.rect = {.x = (int)meta->x, .y = (int)meta->y, .width = (int)meta->w, .height = (int)meta->h};

            auto result = std::make_shared<InferenceResult>();
            result->inference_frame.buffer = buffer;
            result->inference_frame.roi = *meta;
            result->model = &model;

            auto preProcessFunction = model.input_preproc && ovino->use_landmarks
                                          ? InputPreProcess(image, meta, model.input_preproc)
                                          : [](InferenceBackend::Image &) {};

            model.inference->SubmitImage(image, result, preProcessFunction);
        }
    }

    gva_buffer_unmap(buffer, image, mapContext);

    // return FLOW_DROPPED as we push buffers from separate thread
    return GST_BASE_TRANSFORM_FLOW_DROPPED;
}

void ClassifyInference::SinkEvent(GstEvent *event) {
    if (event->type == GST_EVENT_EOS) {
        for (ClassificationModel &model : models) {
            model.inference->Flush();
        }
    }
}

void ClassifyInference::InferenceCompletionCallback(
    std::map<std::string, InferenceBackend::OutputBlob::Ptr> blobs,
    std::vector<std::shared_ptr<InferenceBackend::ImageInference::IFrameBase>> frames) {
    if (frames.empty())
        return;

    std::lock_guard<std::mutex> guard(output_frames_mutex);

    std::vector<InferenceFrame> inference_frames;
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

    Blob2RoiMeta(blobs, inference_frames, inference_id.c_str(), model ? model->model_name.c_str() : nullptr,
                 model->model_proc);

    for (InferenceFrame &frame : inference_frames) {
        for (OutputFrame &output : output_frames) {
            if (frame.buffer == output.buffer || frame.buffer == output.writable_buffer) {
                output.inference_count--;
                break;
            }
        }
    }

    PushOutput();
}

std::function<void(InferenceBackend::Image &)>
ClassifyInference::InputPreProcess(const InferenceBackend::Image &image, GstVideoRegionOfInterestMeta *roi_meta,
                                   GstStructure *preproc) {
    const gchar *converter = gst_structure_get_string(preproc, "converter");
    if (std::string(converter) == "alignment") {
        std::vector<float> reference_points;
        std::vector<float> landmarks_points;
        // look for tensor data with corresponding format
        GVA::RegionOfInterest roi(roi_meta);
        for (auto tensor : roi) {
            if (tensor.get_string("format") == "landmark_points") {
                landmarks_points = tensor.data<float>();
                break;
            }
        }
        // load reference points from JSON input_preproc description
        GValueArray *alignment_points = nullptr;
        if (gst_structure_get_array(preproc, "alignment_points", &alignment_points)) {
            for (size_t i = 0; i < alignment_points->n_values; i++) {
                reference_points.push_back(g_value_get_double(alignment_points->values + i));
            }
        }

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        if (alignment_points)
            g_value_array_free(alignment_points);
        G_GNUC_END_IGNORE_DEPRECATIONS

        if (landmarks_points.size() && landmarks_points.size() == reference_points.size()) {

            return [reference_points, landmarks_points](InferenceBackend::Image &picture) {
                align_rgb_image(picture, landmarks_points, reference_points);
            };
        }
    }
    return [](InferenceBackend::Image &) {};
}
