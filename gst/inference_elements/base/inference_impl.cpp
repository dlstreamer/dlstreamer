/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <assert.h>
#include <cmath>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <ie_plugin_config.hpp>

#include "common/pre_processor_info_parser.hpp"
#include "common/pre_processors.h"
#include "config.h"
#include "core_singleton.h"
#include "gst_allocator_wrapper.h"
#include "gva_buffer_map.h"
#include "gva_caps.h"
#include "gva_utils.h"
#include "inference_backend/logger.h"
#include "inference_backend/pre_proc.h"
#include "inference_backend/safe_arithmetic.h"
#include "inference_impl.h"
#include "logger_functions.h"
#include "model_proc/model_proc_provider.h"
#include "region_of_interest.h"
#include "utils.h"
#include "video_frame.h"

#include <gst/allocators/allocators.h>

#ifdef ENABLE_VAAPI
#include "vaapi_utils.h"
#endif

using namespace std::placeholders;
using namespace InferenceBackend;

namespace {

inline std::shared_ptr<Allocator> CreateAllocator(const char *const allocator_name) {
    std::shared_ptr<Allocator> allocator;
    if (allocator_name != nullptr) {
        allocator = std::make_shared<GstAllocatorWrapper>(allocator_name);
        GVA_TRACE("GstAllocatorWrapper is created");
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

ImagePreprocessorType ImagePreprocessorTypeFromString(const std::string &image_preprocessor_name) {
    constexpr std::pair<const char *, ImagePreprocessorType> preprocessor_types[]{
        {"", ImagePreprocessorType::AUTO},
        {"ie", ImagePreprocessorType::IE},
        {"vaapi", ImagePreprocessorType::VAAPI_SYSTEM},
        {"vaapi-surface-sharing", ImagePreprocessorType::VAAPI_SURFACE_SHARING},
        {"opencv", ImagePreprocessorType::OPENCV}};

    for (auto &elem : preprocessor_types) {
        if (image_preprocessor_name == elem.first)
            return elem.second;
    }

    throw std::runtime_error("Invalid pre-process-backend property value provided: " + image_preprocessor_name +
                             ". Check element's description for supported property values.");
}

uint32_t GetOptimalBatchSize(const char *device) {
    uint32_t batch_size = 1;
    // if the device has the format GPU.x we assume that these are discrete graphics and choose larger batch
    if (device and std::string(device).find("GPU.") != std::string::npos)
        batch_size = 64;
    return batch_size;
}

InferenceConfig CreateNestedInferenceConfig(GvaBaseInference *gva_base_inference, const std::string &model_file) {
    InferenceConfig config;
    std::map<std::string, std::string> base;
    std::map<std::string, std::string> inference = Utils::stringToMap(gva_base_inference->ie_config);

    base[KEY_MODEL] = model_file;
    base[KEY_NIREQ] = std::to_string(gva_base_inference->nireq);
    if (gva_base_inference->device != nullptr) {
        std::string device = gva_base_inference->device;
        base[KEY_DEVICE] = device;
        if (device == "CPU") {
            if (inference.find(KEY_CPU_THROUGHPUT_STREAMS) == inference.end()) {
                inference[KEY_CPU_THROUGHPUT_STREAMS] = (gva_base_inference->cpu_streams == 0)
                                                            ? "CPU_THROUGHPUT_AUTO"
                                                            : std::to_string(gva_base_inference->cpu_streams);
            }
        }
        if (device.find("GPU") != std::string::npos) {
            if (inference.find(KEY_GPU_THROUGHPUT_STREAMS) == inference.end()) {
                inference[KEY_GPU_THROUGHPUT_STREAMS] = (gva_base_inference->gpu_streams == 0)
                                                            ? "GPU_THROUGHPUT_AUTO"
                                                            : std::to_string(gva_base_inference->gpu_streams);
            }
        }
    }

    // gva_base_inference->device_extensions is "device1=extension1,device2=extension2"-like string
    base[KEY_DEVICE_EXTENSIONS] = gva_base_inference->device_extensions;
    base[KEY_PRE_PROCESSOR_TYPE] =
        std::to_string(static_cast<int>(ImagePreprocessorTypeFromString(gva_base_inference->pre_proc_type)));
    base[KEY_IMAGE_FORMAT] =
        GstVideoFormatToString(static_cast<GstVideoFormat>(gva_base_inference->info->finfo->format));

    const uint32_t batch = gva_base_inference->batch_size;
    base[KEY_BATCH_SIZE] = std::to_string(batch);
    base[KEY_RESHAPE] = std::to_string(gva_base_inference->reshape);
    if (gva_base_inference->reshape) {
        if ((gva_base_inference->reshape_width) || (gva_base_inference->reshape_height) || (batch > 1)) {
            base[KEY_RESHAPE_WIDTH] = std::to_string(gva_base_inference->reshape_width);
            base[KEY_RESHAPE_HEIGHT] = std::to_string(gva_base_inference->reshape_height);
        } else {
            base[KEY_RESHAPE_WIDTH] = std::to_string(gva_base_inference->info->width);
            base[KEY_RESHAPE_HEIGHT] = std::to_string(gva_base_inference->info->height);
        }
    }
    base[KEY_CAPS_FEATURE] = std::to_string(static_cast<int>(gva_base_inference->caps_feature));

    config[KEY_BASE] = base;
    config[KEY_INFERENCE] = inference;

    return config;
}

bool DoesModelProcDefinePreProcessing(const std::vector<ModelInputProcessorInfo::Ptr> &model_input_processor_info) {
    for (const auto &it : model_input_processor_info) {
        if (it != nullptr) {
            if (it->format == "image") {
                InputImageLayerDesc::Ptr input_desc = PreProcParamsParser(it->params).parse();
                if (!input_desc)
                    continue;
                if (input_desc->isDefined())
                    return true;
            }
        }
    }
    return false;
}

// Returns default suitable preprocessor according to caps and custom preprocessing options
ImagePreprocessorType ChoosePreferredImagePreproc(CapsFeature caps, bool does_model_proc_define_pre_proc) {
    if (does_model_proc_define_pre_proc)
        return ImagePreprocessorType::OPENCV;

    switch (caps) {
    case SYSTEM_MEMORY_CAPS_FEATURE:
        return ImagePreprocessorType::IE;
    case VA_SURFACE_CAPS_FEATURE:
        return ImagePreprocessorType::VAAPI_SYSTEM;
    case DMA_BUF_CAPS_FEATURE:
#ifdef ENABLE_VPUX
        return ImagePreprocessorType::IE;
#else
        return ImagePreprocessorType::VAAPI_SYSTEM;
#endif
    default:
        throw std::runtime_error("Unsupported caps have been detected for image preprocessor!");
    }
}

void SetPreprocessor(InferenceConfig &config,
                     const std::vector<ModelInputProcessorInfo::Ptr> &model_input_processor_info) {
    const auto caps = static_cast<CapsFeature>(std::stoi(config[KEY_BASE][KEY_CAPS_FEATURE]));

    const auto preferred =
        ChoosePreferredImagePreproc(caps, DoesModelProcDefinePreProcessing(model_input_processor_info));
    const auto current = static_cast<ImagePreprocessorType>(std::stoi(config[KEY_BASE][KEY_PRE_PROCESSOR_TYPE]));

    if (current == ImagePreprocessorType::AUTO) {
        config[KEY_BASE][KEY_PRE_PROCESSOR_TYPE] = std::to_string(static_cast<int>(preferred));
    } else if (preferred == ImagePreprocessorType::OPENCV && current != preferred) {
        throw std::runtime_error(
            "Only OpenCV can be chosen as customizable input preprocessor according to its "
            "description provided in specified model proc. If you want to use another pre-processing, please remove "
            "inappropriate parameters for desired pre-process-backend.");
    }
}

void UpdateConfigWithLayerInfo(const std::vector<ModelInputProcessorInfo::Ptr> &model_input_processor_info,
                               const std::map<std::string, GstStructure *> &model_output_processor_info,
                               std::map<std::string, std::map<std::string, std::string>> &config) {
    std::map<std::string, std::string> layer_precision;
    std::map<std::string, std::string> format;
    for (const ModelInputProcessorInfo::Ptr &preproc : model_input_processor_info) {
        layer_precision[preproc->layer_name] = preproc->precision;
        format[preproc->layer_name] = preproc->format;
    }

    for (const auto &postproc : model_output_processor_info) {
        std::string layer_name = postproc.first;
        layer_precision[layer_name] = std::to_string(static_cast<int>(Blob::Precision::FP32));
    }

    config[KEY_LAYER_PRECISION] = layer_precision;
    config[KEY_FORMAT] = format;
}

void ApplyImageBoundaries(std::shared_ptr<InferenceBackend::Image> &image, GstVideoRegionOfInterestMeta *meta,
                          InferenceRegionType inference_region) {
    if (!meta) {
        throw std::invalid_argument("Region of interest meta is null.");
    }
    if (inference_region == FULL_FRAME) {
        image->rect = Rectangle<uint32_t>(meta->x, meta->y, meta->w, meta->h);
        return;
    }

    const auto image_width = image->width;
    const auto image_height = image->height;

    GVA::RegionOfInterest roi(meta);
    const GVA::Rect<double> normalized_bbox = roi.normalized_rect();

    const constexpr double zero = 0;
    const GVA::Rect<uint32_t> raw_coordinates = {
        .x = static_cast<uint32_t>((std::max(round(normalized_bbox.x * image_width), zero))),
        .y = static_cast<uint32_t>((std::max(round(normalized_bbox.y * image_height), zero))),
        .w = static_cast<uint32_t>((std::max(round(normalized_bbox.w * image_width), zero))),
        .h = static_cast<uint32_t>((std::max(round(normalized_bbox.h * image_height), zero)))};

    image->rect.x = std::min(raw_coordinates.x, image_width);
    image->rect.y = std::min(raw_coordinates.y, image_height);
    image->rect.width = (safe_add(raw_coordinates.w, raw_coordinates.x) > image_width) ? image_width - image->rect.x
                                                                                       : raw_coordinates.w;
    image->rect.height = (safe_add(raw_coordinates.h, raw_coordinates.y) > image_height) ? image_height - image->rect.y
                                                                                         : raw_coordinates.h;
}

std::shared_ptr<InferenceBackend::Image> CreateImage(GstBuffer *buffer, GstVideoInfo *info,
                                                     InferenceBackend::MemoryType mem_type, GstMapFlags map_flags) {
    ITT_TASK(__FUNCTION__);
    try {
        std::unique_ptr<InferenceBackend::Image> unique_image = std::unique_ptr<InferenceBackend::Image>(new Image);
        assert(unique_image.get() != nullptr && "Expected a valid InferenceBackend::Image");

        std::shared_ptr<BufferMapContext> map_context = std::make_shared<BufferMapContext>();
        assert(map_context.get() != nullptr && "Expected a valid BufferMapContext");

        gva_buffer_map(buffer, *unique_image, *map_context, info, mem_type, map_flags);

        auto image_deleter = [map_context](InferenceBackend::Image *image) {
            gva_buffer_unmap(*map_context);
            delete image;
        };
        return std::shared_ptr<InferenceBackend::Image>(unique_image.release(), image_deleter);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to create image from GstBuffer"));
    }
}

void UpdateClassificationHistory(GstVideoRegionOfInterestMeta *meta, GvaBaseInference *gva_base_inference,
                                 const GstStructure *classification_result) {
    GstGvaClassify *gvaclassify = (GstGvaClassify *)gva_base_inference;
    gint meta_id = 0;
    get_object_id(meta, &meta_id);
    if (gvaclassify->reclassify_interval != 1 and meta_id > 0)
        gvaclassify->classification_history->UpdateROIParams(meta_id, classification_result);
}

MemoryType GetMemoryType(CapsFeature caps_feature) {
    MemoryType type = MemoryType::ANY;
    switch (caps_feature) {
    case CapsFeature::SYSTEM_MEMORY_CAPS_FEATURE:
        type = MemoryType::SYSTEM;
        break;
    case CapsFeature::DMA_BUF_CAPS_FEATURE:
#ifdef ENABLE_VPUX
        type = MemoryType::SYSTEM;
#else
        type = MemoryType::DMA_BUFFER;
#endif
        break;
    case CapsFeature::VA_SURFACE_CAPS_FEATURE:
        type = MemoryType::VAAPI;
        break;
    }
    return type;
}

MemoryType GetMemoryType(MemoryType input_image_memory_type, ImagePreprocessorType image_preprocessor_type) {
    MemoryType type = MemoryType::ANY;
    switch (input_image_memory_type) {
    case MemoryType::SYSTEM: {
        switch (image_preprocessor_type) {
        case ImagePreprocessorType::OPENCV:
        case ImagePreprocessorType::IE:
            type = MemoryType::SYSTEM;
            break;
        default:
            throw std::invalid_argument("For system memory only supports ie, opencv image preprocessors");
        }
        break;
    }
    case MemoryType::VAAPI:
    case MemoryType::DMA_BUFFER: {
        switch (image_preprocessor_type) {
        case ImagePreprocessorType::OPENCV:
        case ImagePreprocessorType::IE:
            type = MemoryType::SYSTEM;
            break;
        case ImagePreprocessorType::VAAPI_SURFACE_SHARING:
        case ImagePreprocessorType::VAAPI_SYSTEM:
            type = input_image_memory_type;
            break;
        default:
            throw std::invalid_argument("Invalid image preprocessor type");
        }
        break;
    }
    default:
        break;
    }
    return type;
}

VaApiDisplayPtr createVaDisplay(GvaBaseInference *gva_base_inference) {
#ifdef ENABLE_VAAPI
    uint32_t rel_dev_index = 0;
    const std::string device(gva_base_inference->device);
    if (device.find("GPU") != device.npos)
        rel_dev_index = Utils::getRelativeGpuDeviceIndex(device);
    auto display = vaApiCreateVaDisplay(rel_dev_index);
    return display;
#else
    (void)gva_base_inference;
    return nullptr;
#endif
}

} // namespace

InferenceImpl::Model InferenceImpl::CreateModel(GvaBaseInference *gva_base_inference, const std::string &model_file,
                                                const std::string &model_proc_path) {
    if (not Utils::fileExists(model_file))
        throw std::invalid_argument("Model file '" + model_file + "' does not exist");

    Model model;
    if (!model_proc_path.empty()) {
        const constexpr size_t MAX_MODEL_PROC_SIZE = 10 * 1024 * 1024; // 10 Mb
        if (!Utils::CheckFileSize(model_proc_path, MAX_MODEL_PROC_SIZE))
            throw std::invalid_argument("\'model-proc\' file " + model_proc_path +
                                        "' size exceeds the allowable size (10 MB).");

        ModelProcProvider model_proc_provider;
        model_proc_provider.readJsonFile(model_proc_path);
        model.input_processor_info = model_proc_provider.parseInputPreproc();
        model.output_processor_info = model_proc_provider.parseOutputPostproc();
        // TODO: move code below into model_proc_provider
        for (auto proc : model.output_processor_info) {
            GValueArray *labels_raw = nullptr;
            gst_structure_get_array(proc.second, "labels", &labels_raw);
            gst_structure_remove_field(proc.second, "labels");

            std::vector<std::string> labels;
            if (labels_raw and labels_raw->n_values) {
                labels.reserve(labels_raw->n_values);
                for (uint32_t i = 0; i < labels_raw->n_values; ++i) {
                    const gchar *label = g_value_get_string(labels_raw->values + i);
                    labels.emplace_back(label);
                }
                g_value_array_free(labels_raw);
            }
            model.labels[proc.first] = labels;
        }
    }

    if (gva_base_inference->batch_size == 0)
        gva_base_inference->batch_size = GetOptimalBatchSize(gva_base_inference->device);

    UpdateModelReshapeInfo(gva_base_inference);
    InferenceConfig ie_config = CreateNestedInferenceConfig(gva_base_inference, model_file);
    UpdateConfigWithLayerInfo(model.input_processor_info, model.output_processor_info, ie_config);
    SetPreprocessor(ie_config, model.input_processor_info);
    memory_type =
        GetMemoryType(GetMemoryType(static_cast<CapsFeature>(std::stoi(ie_config[KEY_BASE][KEY_CAPS_FEATURE]))),
                      static_cast<ImagePreprocessorType>(std::stoi(ie_config[KEY_BASE][KEY_PRE_PROCESSOR_TYPE])));

    VaApiDisplayPtr va_dpy;
    if (memory_type == MemoryType::VAAPI || memory_type == MemoryType::DMA_BUFFER) {
        va_dpy = createVaDisplay(gva_base_inference);
    }

    auto image_inference = ImageInference::make_shared(
        memory_type, ie_config, allocator.get(), std::bind(&InferenceImpl::InferenceCompletionCallback, this, _1, _2),
        std::bind(&InferenceImpl::PushFramesIfInferenceFailed, this, _1), std::move(va_dpy));
    if (not image_inference)
        throw std::runtime_error("Failed to create inference instance");
    model.inference = image_inference;
    model.name = image_inference->GetModelName();

    return model;
}

InferenceImpl::InferenceImpl(GvaBaseInference *gva_base_inference) {
    assert(gva_base_inference != nullptr && "Expected a valid pointer to gva_base_inference");
    if (!gva_base_inference->model) {
        throw std::runtime_error("Model not specified");
    }
    std::string model_file(gva_base_inference->model);

    std::string model_proc{};
    if (gva_base_inference->model_proc) {
        model_proc = gva_base_inference->model_proc;
    }

    allocator = CreateAllocator(gva_base_inference->allocator_name);

    GST_WARNING_OBJECT(gva_base_inference, "Loading model: device=%s, path=%s", gva_base_inference->device,
                       model_file.c_str());
    GST_WARNING_OBJECT(gva_base_inference, "Initial settings batch_size=%d, nireq=%d", gva_base_inference->batch_size,
                       gva_base_inference->nireq);
    set_log_function(GST_logger);
    this->model = CreateModel(gva_base_inference, model_file, model_proc);
}

void InferenceImpl::FlushInference() {
    model.inference->Flush();
}

void InferenceImpl::UpdateObjectClasses(GvaBaseInference *gva_base_inference) {
    if (gva_base_inference->object_class && gva_base_inference->object_class[0])
        object_classes = Utils::splitString(gva_base_inference->object_class, ',');
    else
        object_classes.clear();
}

void InferenceImpl::UpdateModelReshapeInfo(GvaBaseInference *gva_base_inference) {
    try {
        if (gva_base_inference->reshape)
            return;

        if (gva_base_inference->reshape_width || gva_base_inference->reshape_height) {
            GVA_WARNING("reshape switched to TRUE because reshape_width or reshape_height more than 0");
            gva_base_inference->reshape = true;
            return;
        }

        if (gva_base_inference->batch_size > 1) {
            GVA_WARNING("reshape switched to TRUE because batch-size more than 1");
            gva_base_inference->reshape = true;
            return;
        }
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to update reshape"));
    }
}

bool InferenceImpl::FilterObjectClass(GstVideoRegionOfInterestMeta *roi) const {
    if (object_classes.empty())
        return true;
    auto compare_quark_string = [roi](const std::string &str) {
        const gchar *roi_type = roi->roi_type ? g_quark_to_string(roi->roi_type) : "";
        return (strcmp(roi_type, str.c_str()) == 0);
    };
    return std::find_if(object_classes.cbegin(), object_classes.cend(), compare_quark_string) != object_classes.cend();
}

bool InferenceImpl::FilterObjectClass(const std::string &object_class) const {
    if (object_classes.empty())
        return true;
    return std::find(object_classes.cbegin(), object_classes.cend(), object_class) != object_classes.cend();
}

InferenceImpl::~InferenceImpl() {
    for (auto proc : model.output_processor_info)
        gst_structure_free(proc.second);
}

void InferenceImpl::PushOutput() {
    ITT_TASK(__FUNCTION__);
    while (!output_frames.empty()) {
        auto &front = output_frames.front();
        if (front.inference_count != 0) {
            break; // inference not completed yet
        }

        for (const std::shared_ptr<InferenceFrame> inference_roi : front.inference_rois) {
            for (const GstStructure *roi_classification : inference_roi->roi_classifications) {
                UpdateClassificationHistory(&inference_roi->roi, front.filter, roi_classification);
            }
        }

        PushBufferToSrcPad(front);
        output_frames.pop_front();
    }
}

void InferenceImpl::PushBufferToSrcPad(OutputFrame &output_frame) {
    GstBuffer *buffer = output_frame.buffer;
    if (output_frame.writable_buffer)
        if (*(output_frame.writable_buffer))
            buffer = *(output_frame.writable_buffer);

    if (!check_gva_base_inference_stopped(output_frame.filter)) {
        GstFlowReturn ret = gst_pad_push(GST_BASE_TRANSFORM_SRC_PAD(output_frame.filter), buffer);
        if (ret != GST_FLOW_OK) {
            std::string err = "Inference gst_pad_push returned status " + std::to_string(ret);
            GVA_WARNING(err.c_str());
        }
    }
}

std::shared_ptr<InferenceImpl::InferenceResult>
InferenceImpl::MakeInferenceResult(GvaBaseInference *gva_base_inference, Model &model,
                                   GstVideoRegionOfInterestMeta *meta, std::shared_ptr<InferenceBackend::Image> &image,
                                   GstBuffer *buffer) {
    auto result = std::make_shared<InferenceResult>();
    /* expect that std::make_shared must throw instead of returning nullptr */
    assert(result.get() != nullptr && "Expected a valid InferenceResult");

    result->inference_frame = std::make_shared<InferenceFrame>();
    /* expect that std::make_shared must throw instead of returning nullptr */
    assert(result->inference_frame.get() != nullptr && "Expected a valid InferenceFrame");

    result->inference_frame->buffer = buffer;
    result->inference_frame->roi = *meta;
    result->inference_frame->gva_base_inference = gva_base_inference;
    if (gva_base_inference->info)
        result->inference_frame->info = gst_video_info_copy(gva_base_inference->info);

    result->model = &model;
    result->image = image;
    return result;
}

GstFlowReturn InferenceImpl::SubmitImages(GvaBaseInference *gva_base_inference,
                                          const std::vector<GstVideoRegionOfInterestMeta *> &metas, GstBuffer *buffer) {
    ITT_TASK(__FUNCTION__);
    try {
        /* we invoke CreateImage::gva_buffer_map::gst_video_frame_map with
         * GST_VIDEO_FRAME_MAP_FLAG_NO_REF to avoid refcount increase.
         * CreateImage::gva_buffer_unmap::gst_video_frame_unmap also will not decrease refcount.
         */
        std::shared_ptr<InferenceBackend::Image> image = CreateImage(
            buffer, gva_base_inference->info, memory_type, GstMapFlags(GST_MAP_READ | GST_VIDEO_FRAME_MAP_FLAG_NO_REF));

        for (const auto meta : metas) {
            ApplyImageBoundaries(image, meta, gva_base_inference->inference_region);
            auto result = MakeInferenceResult(gva_base_inference, model, meta, image, buffer);
            std::map<std::string, InferenceBackend::InputLayerDesc::Ptr> input_preprocessors;
            if (not model.input_processor_info.empty() and gva_base_inference->input_prerocessors_factory)
                input_preprocessors =
                    gva_base_inference->input_prerocessors_factory(model.inference, model.input_processor_info, meta);
            model.inference->SubmitImage(*image, result, input_preprocessors);
        }
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to submit images to inference"));
    }

    // return FLOW_DROPPED as we push buffers from separate thread
    return GST_BASE_TRANSFORM_FLOW_DROPPED;
}

const InferenceImpl::Model &InferenceImpl::GetModel() const {
    return model;
}

GstFlowReturn InferenceImpl::TransformFrameIp(GvaBaseInference *gva_base_inference, GstBuffer *buffer) {
    ITT_TASK(__FUNCTION__);
    std::unique_lock<std::mutex> lock(_mutex);

    assert(gva_base_inference != nullptr && "Expected a valid pointer to gva_base_inference");
    assert(gva_base_inference->info != nullptr && "Expected a valid pointer to GstVideoInfo");

    InferenceStatus status = INFERENCE_EXECUTED;
    {
        ITT_TASK("InferenceImpl::TransformFrameIp check_skip");
        if (++gva_base_inference->num_skipped_frames < gva_base_inference->inference_interval) {
            status = INFERENCE_SKIPPED_PER_PROPERTY;
        }
        if (gva_base_inference->no_block) {
            if (model.inference->IsQueueFull()) {
                status = INFERENCE_SKIPPED_NO_BLOCK;
            }
        }
        if (status == INFERENCE_EXECUTED) {
            gva_base_inference->num_skipped_frames = 0;
        }
    }

    /* Collect all ROI metas into std::vector */
    std::vector<GstVideoRegionOfInterestMeta *> metas;
    GstVideoRegionOfInterestMeta full_frame_meta;
    {
        ITT_TASK("InferenceImpl::TransformFrameIp collectROIMetas");
        switch (gva_base_inference->inference_region) {
        case ROI_LIST: {
            /* iterates through buffer's meta and pushes it in vector if inference needed. */
            GstVideoRegionOfInterestMeta *meta = NULL;
            gpointer state = NULL;
            while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
                if (!gva_base_inference->is_roi_inference_needed ||
                    gva_base_inference->is_roi_inference_needed(gva_base_inference, gva_base_inference->frame_num,
                                                                buffer, meta)) {
                    metas.push_back(meta);
                }
            }
            break;
        }
        case FULL_FRAME: {
            /* pushes single meta in vector if full-frame inference is invoked. */
            full_frame_meta = GstVideoRegionOfInterestMeta();
            full_frame_meta.x = 0;
            full_frame_meta.y = 0;
            full_frame_meta.w = gva_base_inference->info->width;
            full_frame_meta.h = gva_base_inference->info->height;
            metas.push_back(&full_frame_meta);
            break;
        }
        default:
            throw std::logic_error("Unsupported inference region type");
        }
    }

    // count number ROIs to run inference on
    size_t inference_count = (status == INFERENCE_EXECUTED) ? metas.size() : 0;
    gva_base_inference->frame_num++;
    if (gva_base_inference->frame_num == G_MAXUINT64) {
        GST_WARNING_OBJECT(gva_base_inference,
                           "The frame counter value limit has been reached. This value will be reset.");
    }

    // push into output_frames queue
    {
        ITT_TASK("InferenceImpl::TransformFrameIp pushIntoOutputFramesQueue");
        std::lock_guard<std::mutex> guard(output_frames_mutex);
        if (!inference_count && output_frames.empty()) {
            // If we don't need to run inference and there are no frames queued for inference then finish transform
            return GST_FLOW_OK;
        }

        InferenceImpl::OutputFrame output_frame = {.buffer = buffer,
                                                   .writable_buffer = NULL,
                                                   .inference_count = inference_count,
                                                   .filter = gva_base_inference,
                                                   .inference_rois = {}};
        output_frames.push_back(output_frame);

        /* increment buffer reference to not lose it after transformIp ends */
        gst_buffer_ref(buffer);

        if (!inference_count) {
            return GST_BASE_TRANSFORM_FLOW_DROPPED;
        }
    }

    return SubmitImages(gva_base_inference, metas, buffer);
}

void InferenceImpl::SinkEvent(GstEvent *event) {
    if (event->type == GST_EVENT_EOS) {
        model.inference->Flush();
    }
}

void InferenceImpl::PushFramesIfInferenceFailed(
    std::vector<std::shared_ptr<InferenceBackend::ImageInference::IFrameBase>> frames) {
    std::lock_guard<std::mutex> guard(output_frames_mutex);
    for (auto &frame : frames) {
        auto inference_result = std::dynamic_pointer_cast<InferenceResult>(frame);
        /* InferenceResult is inherited from IFrameBase */
        assert(inference_result.get() != nullptr && "Expected a valid InferenceResult");

        std::shared_ptr<InferenceFrame> inference_roi = inference_result->inference_frame;
        auto it =
            std::find_if(output_frames.begin(), output_frames.end(), [inference_roi](const OutputFrame &output_frame) {
                return output_frame.buffer == inference_roi->buffer;
            });

        if (it != output_frames.end())
            continue;

        PushBufferToSrcPad(*it);
        output_frames.erase(it);
    }
}

/**
 * Updates buffer pointers for corresponding to 'inference_roi' output_frame, decreases it's inference_count.
 * May affect buffer if it's not writable.
 *
 * @param[in] inference_roi - InferenceFrame to provide buffer's and inference element's info
 */
void InferenceImpl::UpdateOutputFrames(std::shared_ptr<InferenceFrame> &inference_roi) {
    /* we must iterate through std::list because it has no lookup operations */
    for (auto &output_frame : output_frames) {
        if (output_frame.buffer != inference_roi->buffer)
            continue;

        /* only gvadetect or full-frame elements are affecting buffer */
        if (inference_roi->gva_base_inference->type == GST_GVA_DETECT_TYPE ||
            inference_roi->gva_base_inference->inference_region == FULL_FRAME) {
            if (output_frame.inference_count == 0)
                // This condition is necessary if two items in output_frames refer to the same buffer.
                // If current output_frame.inference_count equals 0, then inference for this output_frame
                // already happened, but buffer wasn't pushed further by pipeline yet. We skip this buffer
                // to find another, to which current inference callback really belongs
                continue;
            if (!output_frame.writable_buffer || !*(output_frame.writable_buffer)) {
                // check if we have writable version of this buffer (this function called multiple times
                // on same buffer)
                output_frame.writable_buffer = &inference_roi->buffer;
            }
        }
        output_frame.inference_rois.push_back(inference_roi);
        --output_frame.inference_count;
        break;
    }
}

/**
 * Callback called when the inference request is completed. Updates output_frames and invokes post-processing for
 * corresponding inference element then makes gst_pad_push to send buffer further down the pipeline.
 * Acquires output_frames_mutex with std::lock_guard.
 * Nullifies shared_ptr for InferenceBackend::Image created during 'SubmitImages'.
 *
 * @param[in] blobs - the resulting blobs obtained after executing inference
 * @param[in] frames - frames for which an inference was executed
 *
 * @throw throw std::runtime_error when post-processing is failed
 */
void InferenceImpl::InferenceCompletionCallback(
    std::map<std::string, InferenceBackend::OutputBlob::Ptr> blobs,
    std::vector<std::shared_ptr<InferenceBackend::ImageInference::IFrameBase>> frames) {
    std::lock_guard<std::mutex> guard(output_frames_mutex);
    ITT_TASK(__FUNCTION__);
    if (frames.empty())
        return;

    std::vector<std::shared_ptr<InferenceFrame>> inference_frames;
    PostProcessor *post_proc = nullptr;

    for (auto &frame : frames) {
        auto inference_result = std::dynamic_pointer_cast<InferenceResult>(frame);
        /* InferenceResult is inherited from IFrameBase */
        assert(inference_result.get() != nullptr && "Expected a valid InferenceResult");

        std::shared_ptr<InferenceFrame> inference_roi = inference_result->inference_frame;
        inference_roi->image_transform_info = frame->GetImageTransformationParams();
        inference_result->image.reset(); // deleter will to not make buffer_unref, see 'SubmitImages' method
        post_proc = inference_roi->gva_base_inference->post_proc;

        UpdateOutputFrames(inference_roi);
        inference_frames.push_back(inference_roi);
    }

    try {
        if (post_proc != nullptr) {
            PostProcessorExitStatus pp_e_s = post_proc->process(blobs, inference_frames);
            if (pp_e_s == PostProcessorExitStatus::FAIL)
                throw std::runtime_error("Post-processing has been exited with FAIL code.");
        }
    } catch (const std::exception &e) {
        GST_ERROR("%s", Utils::createNestedErrorMsg(e).c_str());
    }

    PushOutput();
}
