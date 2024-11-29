/*******************************************************************************
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "meta_attacher.h"

#include "gva_utils.h"
#include "processor_types.h"
#include <dlstreamer/gst/videoanalytics/objectdetectionmtdext.h>
#include <gst/analytics/analytics.h>

#include <exception>

using namespace post_processing;

MetaAttacher::Ptr MetaAttacher::create(ConverterType converter_type, AttachType attach_type) {
    switch (converter_type) {
    case ConverterType::TO_ROI:
        return MetaAttacher::Ptr(new ROIToFrameAttacher());
    case ConverterType::RAW:
    case ConverterType::TO_TENSOR: {
        switch (attach_type) {
        case AttachType::TO_FRAME:
            return MetaAttacher::Ptr(new TensorToFrameAttacher());
        case AttachType::TO_ROI:
            return MetaAttacher::Ptr(new TensorToROIAttacher());
        case AttachType::FOR_MICRO:
            return MetaAttacher::Ptr(new TensorToFrameAttacherForMicro());
        default:
            throw std::runtime_error("Unknown inference region");
        }
    }
    default:
        throw std::runtime_error("Unknown inference type");
    }
}

void ROIToFrameAttacher::attach(const TensorsTable &tensors, FramesWrapper &frames) {
    checkFramesAndTensorsTable(frames, tensors);

    for (size_t i = 0; i < frames.size(); ++i) {
        auto &frame = frames[i];
        const auto &tensor = tensors[i];

        for (size_t j = 0; j < tensor.size(); ++j) {
            GstStructure *detection_tensor = tensor[j][DETECTION_TENSOR_ID];

            uint32_t x_abs, y_abs, w_abs, h_abs;
            gst_structure_get_uint(detection_tensor, "x_abs", &x_abs);
            gst_structure_get_uint(detection_tensor, "y_abs", &y_abs);
            gst_structure_get_uint(detection_tensor, "w_abs", &w_abs);
            gst_structure_get_uint(detection_tensor, "h_abs", &h_abs);

            const gchar *label = gst_structure_get_string(detection_tensor, "label");

            GstBuffer **writable_buffer = &frame.buffer;
            gva_buffer_check_and_make_writable(writable_buffer, PRETTY_FUNCTION_NAME);

            if (NEW_METADATA) {
                GQuark gquark_label = g_quark_from_string(label);

                gdouble conf;
                gst_structure_get_double(detection_tensor, "confidence", &conf);

                GstAnalyticsRelationMeta *relation_meta = gst_buffer_add_analytics_relation_meta(*writable_buffer);

                if (not relation_meta)
                    throw std::runtime_error("Failed to add GstAnalyticsRelationMeta to buffer");

                GstAnalyticsODMtd od_mtd;
                if (!gst_analytics_relation_meta_add_od_mtd(relation_meta, gquark_label, x_abs, y_abs, w_abs, h_abs,
                                                            conf, &od_mtd)) {
                    throw std::runtime_error("Failed to add detection data to meta");
                }

                gint label_id = 0;
                gst_structure_get_int(detection_tensor, "label_id", &label_id);

                gdouble rotation = 0;
                gst_structure_get_double(detection_tensor, "rotation", &rotation);

                gst_structure_remove_field(detection_tensor, "label");
                gst_structure_remove_field(detection_tensor, "x_abs");
                gst_structure_remove_field(detection_tensor, "y_abs");
                gst_structure_remove_field(detection_tensor, "w_abs");
                gst_structure_remove_field(detection_tensor, "h_abs");

                GstAnalyticsODExtMtd od_ext_mtd;
                if (!gst_analytics_relation_meta_add_od_ext_mtd(relation_meta, rotation, label_id, &od_ext_mtd)) {
                    throw std::runtime_error("Failed to add detection extended data to meta");
                }

                for (size_t k = 0; k < tensor[j].size(); k++) {
                    GstAnalyticsMtd mtd;
                    GVA::Tensor gva_tensor(tensor[j][k]);
                    if (gva_tensor.convert_to_meta(&mtd, relation_meta)) {
                        if (!gst_analytics_relation_meta_set_relation(relation_meta, GST_ANALYTICS_REL_TYPE_RELATE_TO,
                                                                      od_mtd.id, mtd.id)) {
                            throw std::runtime_error(
                                "Failed to set relation between object detection metadata and keypoint metadata");
                        }
                    } else {
                        gst_analytics_od_ext_mtd_add_param(&od_ext_mtd, tensor[j][k]);
                        frames[i].roi_classifications->push_back(tensor[j][k]);
                    }
                }

                if (!gst_analytics_relation_meta_set_relation(relation_meta, GST_ANALYTICS_REL_TYPE_RELATE_TO,
                                                              od_mtd.id, od_ext_mtd.id)) {
                    throw std::runtime_error(
                        "Failed to set relation between object detection metadata and extended metadata");
                }

                GstAnalyticsODMtd parent_od_mtd;
                if (gst_analytics_relation_meta_get_od_mtd(relation_meta, frame.roi->id, &parent_od_mtd)) {
                    if (!gst_analytics_relation_meta_set_relation(relation_meta, GST_ANALYTICS_REL_TYPE_IS_PART_OF,
                                                                  od_mtd.id, parent_od_mtd.id)) {
                        throw std::runtime_error(
                            "Failed to set relation between object detection metadata and parent metadata");
                    }
                }

                continue;
            }

            GstVideoRegionOfInterestMeta *roi_meta =
                gst_buffer_add_video_region_of_interest_meta(*writable_buffer, label, x_abs, y_abs, w_abs, h_abs);

            if (not roi_meta)
                throw std::runtime_error("Failed to add GstVideoRegionOfInterestMeta to buffer");

            roi_meta->id = gst_util_seqnum_next();
            if (frame.roi)
                roi_meta->parent_id = frame.roi->id;

            gst_structure_remove_field(detection_tensor, "label");
            gst_structure_remove_field(detection_tensor, "x_abs");
            gst_structure_remove_field(detection_tensor, "y_abs");
            gst_structure_remove_field(detection_tensor, "w_abs");
            gst_structure_remove_field(detection_tensor, "h_abs");

            gst_video_region_of_interest_meta_add_param(roi_meta, detection_tensor);

            // add tensors other than detection_tensor
            for (size_t k = 1; k < tensor[j].size(); k++) {
                gst_video_region_of_interest_meta_add_param(roi_meta, tensor[j][k]);
                frames[i].roi_classifications->push_back(tensor[j][k]);
            }
        }
    }
}

void TensorToFrameAttacher::attach(const TensorsTable &tensors_batch, FramesWrapper &frames) {
    checkFramesAndTensorsTable(frames, tensors_batch);

    for (size_t i = 0; i < frames.size(); ++i) {
        GstBuffer **writable_buffer = &frames[i].buffer;

        for (std::vector<GstStructure *> tensor_data : tensors_batch[i]) {
            gva_buffer_check_and_make_writable(writable_buffer, PRETTY_FUNCTION_NAME);
            GstGVATensorMeta *tensor = GST_GVA_TENSOR_META_ADD(*writable_buffer);
            /* Tensor Meta already creates GstStructure during initialization */
            /* TODO: reduce amount of GstStructures copy from loading model-proc till attaching meta */
            if (tensor->data) {
                gst_structure_free(tensor->data);
            }
            assert(tensor_data.size() == 1);
            tensor->data = tensor_data[0];
            gst_structure_set(tensor->data, "element_id", G_TYPE_STRING, frames[i].model_instance_id.c_str(), NULL);
        }
    }
}

void TensorToROIAttacher::attach(const TensorsTable &tensors_batch, FramesWrapper &frames) {
    checkFramesAndTensorsTable(frames, tensors_batch);

    for (size_t i = 0; i < frames.size(); ++i) {
        GstBuffer *buffer = frames[i].buffer;

        if (NEW_METADATA) {
            GstAnalyticsODMtd od_meta;
            if (!findODMeta(buffer, frames[i].roi, &od_meta)) {
                GST_WARNING("No detection tensors were found for this buffer in case of roi-list inference.");
                continue;
            }

            GstAnalyticsODExtMtd od_ext_meta;
            if (!gst_analytics_relation_meta_get_direct_related(od_meta.meta, od_meta.id,
                                                                GST_ANALYTICS_REL_TYPE_RELATE_TO,
                                                                GST_ANALYTICS_MTD_TYPE_ANY, nullptr, &od_ext_meta)) {
                throw std::runtime_error("Object detection extended metadata not found");
            }

            for (std::vector<GstStructure *> tensor_data : tensors_batch[i]) {
                assert(tensor_data.size() == 1);
                gst_analytics_od_ext_mtd_add_param(&od_ext_meta, tensor_data[0]);
                frames[i].roi_classifications->push_back(tensor_data[0]);
            }
        } else {
            GstVideoRegionOfInterestMeta *roi_meta = findROIMeta(buffer, frames[i].roi);
            if (!roi_meta) {
                GST_WARNING("No detection tensors were found for this buffer in case of roi-list inference.");
                continue;
            }

            for (std::vector<GstStructure *> tensor_data : tensors_batch[i]) {
                assert(tensor_data.size() == 1);
                gst_video_region_of_interest_meta_add_param(roi_meta, tensor_data[0]);
                frames[i].roi_classifications->push_back(tensor_data[0]);
            }
        }
    }
}

void TensorToFrameAttacherForMicro::attach(const TensorsTable &tensors, FramesWrapper &frames) {

    if (tensors.size() == 0) {
        return;
    }

    // TODO: adjust for batch size
    if (frames.size() != 1) {
        throw std::runtime_error("Failed to attach tensor to frame: Batch size is not supported in micro currently.");
    }
    for (size_t i = 0; i < frames.size(); ++i) {
        auto &frame = frames[i];

        for (std::vector<GstStructure *> tensor_data : tensors[i]) {
            auto tensor = GST_GVA_TENSOR_META_ADD(frame.buffer);
            if (tensor->data) {
                gst_structure_free(tensor->data);
            }
            assert(tensor_data.size() == 1);
            tensor->data = tensor_data[0];
            gst_structure_set(tensor->data, "element_id", G_TYPE_STRING, frame.model_instance_id.c_str(), NULL);
        }
    }
}

GstVideoRegionOfInterestMeta *TensorToROIAttacher::findROIMeta(GstBuffer *buffer,
                                                               GstVideoRegionOfInterestMeta *frame_roi) {
    GstVideoRegionOfInterestMeta *meta = nullptr;
    gpointer state = nullptr;
    while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
        if (sameRegion(meta, frame_roi)) {
            return meta;
        }
    }
    return meta;
}

bool TensorToROIAttacher::findODMeta(GstBuffer *buffer, GstVideoRegionOfInterestMeta *frame_roi,
                                     GstAnalyticsODMtd *rlt_mtd) {
    gpointer state = nullptr;
    GstAnalyticsRelationMeta *relation_meta = gst_buffer_get_analytics_relation_meta(buffer);

    if (relation_meta) {
        while (
            gst_analytics_relation_meta_iterate(relation_meta, &state, gst_analytics_od_mtd_get_mtd_type(), rlt_mtd)) {
            if (sameRegion(rlt_mtd, frame_roi)) {
                return true;
            }
        }
    }
    return false;
}
