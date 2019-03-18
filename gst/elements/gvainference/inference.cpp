/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference.h"

#include <assert.h>
#include <gst/allocators/gstdmabuf.h>
#include <thread>

#include "config.h"
#include "gstgvainference.h"
#include "gva_buffer_map.h"
#include "gva_roi_meta.h"
#include "gva_tensor_meta.h"
#include "gva_utils.h"
#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "logger_functions.h"
#include "read_model_proc.h"

using namespace std::placeholders;

const char *get_human_readable_layout(const InferenceBackend::OutputBlob::Layout &layout) {
    switch (layout) {
#define RET_LAYOUT(name)                                                                                               \
    case name:                                                                                                         \
        return #name;
        RET_LAYOUT(InferenceBackend::OutputBlob::Layout::ANY);
        RET_LAYOUT(InferenceBackend::OutputBlob::Layout::NCHW);
        RET_LAYOUT(InferenceBackend::OutputBlob::Layout::NHWC);
#undef RET_LAYOUT
    default:
        return "UNKNOWN";
    };
}

std::map<std::string, InferenceRefs *> Inference::inference_pool_;
std::mutex Inference::inference_pool_mutex_;

InferenceProxy *Inference::aquire_instance(GstGvaInference *ovino) {
    std::lock_guard<std::mutex> guard(inference_pool_mutex_);
    std::string name(ovino->inference_id);
    InferenceRefs *infRefs = nullptr;
    auto it = inference_pool_.find(name);
    if (it == inference_pool_.end()) {
        infRefs = new InferenceRefs;
        infRefs->numRefs++;
        infRefs->proxy = new InferenceProxy;
        if (ovino->model) {
            infRefs->proxy->instance = new Inference(ovino);

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
            infRefs->proxy->instance = new Inference(ovino);

            // save master element for later reference from slave elements
            infRefs->masterElement = ovino;

            // iterate through already created elements and set their props
            Inference::initExistingElements(infRefs);
            infRefs->elementsToInit.clear();

        } else if (infRefs->proxy->instance && !ovino->model) {
            // TODO add warning saying that specified props will be ignored if specified for slave.
            // Setting missing props to current elem
            Inference::fillElementProps(ovino, infRefs->masterElement);
        } else if (infRefs->proxy->instance && ovino->model) {
            GST_WARNING_OBJECT(ovino, "Only one element for each inference-id can have other properties specified.");
            // TODO implement comprehensive validation if all options match btw slave and master and if not:
            // throw std::runtime_error("Only one element for each inference-id can have other properties specified.");
        }
    }
    return infRefs->proxy;
}

void Inference::release_instance(GstGvaInference *ovino) {
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

void Inference::fillElementProps(GstGvaInference *targetElem, GstGvaInference *masterElem) {
    assert(masterElem);
    g_free(targetElem->model);
    targetElem->model = g_strdup(masterElem->model);
    g_free(targetElem->device);
    targetElem->device = g_strdup(masterElem->device);
    targetElem->batch_size = masterElem->batch_size;
    targetElem->threshold = masterElem->threshold;
    targetElem->resize_by_inference = masterElem->resize_by_inference;
    targetElem->every_nth_frame = masterElem->every_nth_frame;
    targetElem->nireq = masterElem->nireq;
    g_free(targetElem->cpu_extension);
    targetElem->cpu_extension = g_strdup(masterElem->cpu_extension);
    g_free(targetElem->gpu_extension);
    targetElem->gpu_extension = g_strdup(masterElem->gpu_extension);
    // no need to copy inference_id because it should match already.
}

void Inference::initExistingElements(InferenceRefs *infRefs) {
    assert(infRefs->masterElement);
    for (auto elem : infRefs->elementsToInit) {
        Inference::fillElementProps(elem, infRefs->masterElement);
    }
}

InferenceProxy *aquire_inference(GstGvaInference *ovino, GError **error) {
    InferenceProxy *proxy = nullptr;
    try {
        proxy = Inference::aquire_instance(ovino);
    } catch (const std::exception &exc) {
        g_set_error(error, 1, 1, "%s", exc.what());
    }
    return proxy;
}

void release_inference(GstGvaInference *ovino) {
    Inference::release_instance(ovino);
}

void inference_sink_event(GstGvaInference *ovino, GstEvent *event) {
    ovino->inference->instance->SinkEvent(event);
}

GstFlowReturn frame_to_inference(GstGvaInference *ovino, GstBaseTransform *trans, GstBuffer *buf, GstVideoInfo *info) {
    if (!ovino || !ovino->inference || !ovino->inference->instance) {
        GST_ERROR_OBJECT(ovino, "empty inference instance!!!!");
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }
    return ovino->inference->instance->TransformFrameIp(ovino, trans, buf, info);
}

void flush_inference(GstGvaInference *ovino) {
    if (!ovino || !ovino->inference || !ovino->inference->instance) {
        GST_ERROR_OBJECT(ovino, "empty inference instance!!!!");
        return;
    }

    ovino->inference->instance->FlushInference();
}

void ExtractBoundingBoxes(const gchar *model_name, const gchar *layer_name, InferenceBackend::OutputBlob::Ptr blob,
                          const std::vector<InferenceFrame> &frames, const std::vector<std::pair<int, int>> sizes,
                          float threshold, GstStructure *post_proc) {
    const float *detections = (const float *)blob->GetData();
    auto dims = blob->GetDims();
    auto layout = blob->GetLayout();

    GST_DEBUG("DIMS:\n");
    for (auto dim = dims.begin(); dim < dims.end(); dim++) {
        GST_DEBUG("\t%lu\n", *dim);
    }

    gint object_size = 0;
    gint max_proposal_count = 0;
    switch (layout) {
    case InferenceBackend::OutputBlob::Layout::NCHW:
        object_size = dims[3];
        max_proposal_count = dims[2];
        break;
    default:
        GST_ERROR("Got unsupported layout. Boxes won't be extracted\n");
        return;
    }
    if (object_size != 7) { // Intel Model Zoo DetectionOutput format
        GST_ERROR("Thrown away latest blob, object_size is %d. Boxes won't be extracted\n", object_size);
        return;
    }

    GValueArray *labels = nullptr;
    if (post_proc)
        gst_structure_get_array(post_proc, "labels", &labels);

    for (int i = 0; i < max_proposal_count; i++) {
        int image_id = (int)detections[i * object_size + 0];
        int label_id = (int)detections[i * object_size + 1];
        double confidence = detections[i * object_size + 2];
        double x_min = detections[i * object_size + 3];
        double y_min = detections[i * object_size + 4];
        double x_max = detections[i * object_size + 5];
        double y_max = detections[i * object_size + 6];
        if (image_id < 0 || (size_t)image_id >= frames.size()) {
            break;
        }
        if (confidence < threshold) {
            continue;
        }
        int width = sizes[image_id].first;
        int height = sizes[image_id].second;

        const gchar *label = NULL;
        if (labels && label_id >= 0 && label_id < (gint)labels->n_values) {
            label = g_value_get_string(labels->values + label_id);
        }
        gint ix_min = (gint)(x_min * width + 0.5);
        gint iy_min = (gint)(y_min * height + 0.5);
        gint ix_max = (gint)(x_max * width + 0.5);
        gint iy_max = (gint)(y_max * height + 0.5);
        if (ix_min < 0)
            ix_min = 0;
        if (iy_min < 0)
            iy_min = 0;
        if (ix_max > width)
            ix_max = width;
        if (iy_max > height)
            iy_max = height;
        GstVideoRegionOfInterestMeta *meta = gst_buffer_add_video_region_of_interest_meta(
            frames[image_id].buffer, label, ix_min, iy_min, ix_max - ix_min, iy_max - iy_min);

        GstStructure *s = gst_structure_new(
            "detection", "confidence", G_TYPE_DOUBLE, confidence, "label_id", G_TYPE_INT, label_id, "x_min",
            G_TYPE_DOUBLE, x_min, "x_max", G_TYPE_DOUBLE, x_max, "y_min", G_TYPE_DOUBLE, y_min, "y_max", G_TYPE_DOUBLE,
            y_max, "model_name", G_TYPE_STRING, model_name, "layer_name", G_TYPE_STRING, layer_name, NULL);
        gst_video_region_of_interest_meta_add_param(meta, s);
    }

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    if (labels)
        g_value_array_free(labels);
    G_GNUC_END_IGNORE_DEPRECATIONS
}

void Inference::InferenceCompletionCallback(
    std::map<std::string, InferenceBackend::OutputBlob::Ptr> blobs,
    std::vector<std::shared_ptr<InferenceBackend::ImageInference::IFrameBase>> frames) {
    // convert into vector<InferenceFrame>
    std::vector<InferenceFrame> inf_frames;
    std::vector<std::pair<int, int>> sizes;
    for (auto frame : frames) {
        auto frame2 = std::dynamic_pointer_cast<InferenceResult>(frame);
        // make buffer writable as we need to attach metadata
        if (!gst_buffer_is_writable(frame2->inference_frame.buffer)) {
            GST_WARNING("Make buffer available for writing");
            GstBuffer *original = frame2->inference_frame.buffer;
            GstBuffer *writable = gst_buffer_make_writable(original);
            frame2->inference_frame.buffer = writable;
            for (OutputFrame &output_frame : frame2->output_frames) {
                if (output_frame.buffer == original) {
                    output_frame.buffer = writable;
                    break;
                }
            }
        }

        inf_frames.push_back(frame2->inference_frame);
        sizes.push_back(frame2->inference_frame_size);
    }

    GST_DEBUG("BLOBS COUNT %zu\n", blobs.size());

    bool bUnknownOutput = false;
    for (const auto &blob_desc : blobs) {
        InferenceBackend::OutputBlob::Ptr blob = blob_desc.second;
        const std::string &layer_name = blob_desc.first;
        const std::string &layer_type = image_inference->GetLayerTypeByLayerName(layer_name);

        if (layer_type == "DetectionOutput") {
            ExtractBoundingBoxes(ovino_->model, layer_name.data(), blob, inf_frames, sizes, ovino_->threshold,
                                 post_proc);
        } else {
            bUnknownOutput = true;
        }
    }
    if (bUnknownOutput) {
        Blob2TensorMeta(blobs, inf_frames, ovino_->inference_id, ovino_->model);
    }

    // push frames to downstream Gst element
    for (auto frame : frames) {
        auto frame2 = std::dynamic_pointer_cast<InferenceResult>(frame);
        for (OutputFrame &output_frame : frame2->output_frames) {
            GstFlowReturn ret = gst_pad_push(GST_BASE_TRANSFORM_SRC_PAD(output_frame.filter), output_frame.buffer);
            if (ret != GST_FLOW_OK) {
                GST_WARNING("Inference gst_pad_push returned status %d", ret);
            }
        }
    }
}

Inference::Inference(GstGvaInference *ovino) : frame_num(-1), post_proc(nullptr), ovino_(ovino) {
    GST_WARNING_OBJECT(ovino, "Loading model: device=%s, path=%s", ovino->device, ovino->model);
    GST_WARNING_OBJECT(ovino, "Setting batch_size=%d, nireq=%d", ovino->batch_size, ovino->nireq);

    std::map<std::string, std::string> infer_config = String2Map(ovino->infer_config);
    if (ovino->resize_by_inference)
        infer_config[InferenceBackend::KEY_RESIZE_BY_INFERENCE] = "TRUE";
    if (ovino->cpu_extension && *ovino->cpu_extension)
        infer_config[InferenceBackend::KEY_CPU_EXTENSION] = ovino->cpu_extension;
    if (ovino->cpu_streams && *ovino->cpu_streams) {
        std::string cpu_streams = ovino->cpu_streams;
        if (cpu_streams == "true")
            cpu_streams = std::to_string(ovino->nireq);
        if (cpu_streams != "false")
            infer_config[InferenceBackend::KEY_CPU_THROUGHPUT_STREAMS] = cpu_streams;
    }

    set_log_function(GST_logger);

    image_inference = InferenceBackend::ImageInference::make_shared(
        InferenceBackend::MemoryType::ANY, ovino->device, ovino->model, ovino->batch_size, ovino->nireq, infer_config,
        std::bind(&Inference::InferenceCompletionCallback, this, _1, _2));

    if (ovino->model_proc) {
        auto model_proc = ReadModelProc(ovino->model_proc);
        for (auto proc : model_proc) {
            if (gst_structure_has_field(proc.second, "labels") && !post_proc) {
                post_proc = proc.second;
            } else {
                gst_structure_free(proc.second);
            }
        }
    }
}

Inference::~Inference() {
    image_inference->Close();
    if (post_proc)
        gst_structure_free(post_proc);
}

void Inference::FlushInference() {
    image_inference->Flush();
}

GstFlowReturn Inference::TransformFrameIp(GstGvaInference *ovino, GstBaseTransform *trans, GstBuffer *buffer,
                                          GstVideoInfo *info) {
    std::unique_lock<std::mutex> lock(_mutex);

    // decide we run inference or frame goes pass-through without inference
    bool bRunInference = false;
    frame_num++;
    if (ovino->every_nth_frame && !(frame_num % ovino->every_nth_frame)) {
        bRunInference = true;
    }

    // Create new queue
    if (result == nullptr) {
        result = std::make_shared<InferenceResult>();
    }

    // increment reference
    buffer = gst_buffer_ref(buffer);

    result->output_frames.push_back({.buffer = buffer, .filter = trans});

    if (bRunInference) {
        result->inference_frame.buffer = buffer;
        result->inference_frame_size = std::pair<int, int>(info->width, info->height);

        InferenceBackend::Image image;
        BufferMapContext mapContext;
        gva_buffer_map(buffer, image, mapContext, info, InferenceBackend::MemoryType::ANY);
        image_inference->SubmitImage(image, result, [](InferenceBackend::Image &) {});
        gva_buffer_unmap(buffer, image, mapContext);

        result = nullptr;
    }

    // return FLOW_DROPPED as we push buffers from separate thread
    return GST_BASE_TRANSFORM_FLOW_DROPPED;
}

void Inference::SinkEvent(GstEvent *event) {
    if (event->type == GST_EVENT_EOS) {
        image_inference->Flush();
    }
}
