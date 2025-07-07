/*******************************************************************************
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "common/pre_processors.h"
#include "utils.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

const std::string image_format = "image";
const std::string layer1_name = "layer1";

const std::string image_info_format = "image_info";
const std::string layer2_name = "layer2";

const std::string u8 = "U8";

using namespace InferenceBackend;

class MockImageInference : public ImageInference {
  public:
    MOCK_METHOD0(Init, void());
    MOCK_METHOD2(SubmitImage, void(IFrameBase::Ptr frame,
                                   const std::map<std::string, std::shared_ptr<InputLayerDesc>> &input_preprocessors));
    MOCK_CONST_METHOD0(GetModelName, const std::string &());
    MOCK_CONST_METHOD0(GetBatchSize, size_t());
    MOCK_CONST_METHOD0(GetNireq, size_t());
    MOCK_CONST_METHOD5(GetModelImageInputInfo,
                       void(size_t &width, size_t &height, size_t &batch_size, int &format, int &memory_type));
    MOCK_CONST_METHOD0(GetModelInputsInfo, std::map<std::string, std::vector<size_t>>());
    MOCK_CONST_METHOD0(GetModelOutputsInfo, std::map<std::string, std::vector<size_t>>());
    MOCK_CONST_METHOD0(GetModelInfoPostproc, std::map<std::string, GstStructure *>());

    MOCK_METHOD0(IsQueueFull, bool());
    MOCK_METHOD0(Flush, void());
    MOCK_METHOD0(Close, void());
};

class MockInputBlob : public InputBlob {
  public:
    using Ptr = std::shared_ptr<MockInputBlob>;

    MOCK_METHOD0(GetData, void *());
    MOCK_CONST_METHOD0(GetIndexInBatch, size_t());
    MOCK_CONST_METHOD0(GetDims, const std::vector<size_t> &());
    MOCK_CONST_METHOD0(GetLayout, Layout());
    MOCK_CONST_METHOD0(GetPrecision, Precision());
};

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::ReturnRef;

struct CommonPreprocTest : public testing::Test {
    std::shared_ptr<MockImageInference> image_inference;
    GstVideoRegionOfInterestMeta roi;

    std::shared_ptr<ModelInputProcessorInfo> CreateModelInputProcessorInfo(const std::string &layer_name,
                                                                           GstStructure *params,
                                                                           const std::string &format = image_format,
                                                                           const std::string &precision = u8) {
        std::shared_ptr<ModelInputProcessorInfo> preprocessor(new ModelInputProcessorInfo);
        preprocessor->layer_name = layer_name;
        preprocessor->format = format;
        preprocessor->precision = precision;
        preprocessor->params = params;

        return preprocessor;
    }

    void SetUp() {
        image_inference = std::make_shared<MockImageInference>();
        roi = GstVideoRegionOfInterestMeta();
    }
};

TEST_F(CommonPreprocTest, EmptyInputModelParams) {
    std::vector<ModelInputProcessorInfo::Ptr> input_model_proc;
    GstStructure *params = gst_structure_new_empty("params");
    input_model_proc.push_back(CreateModelInputProcessorInfo(layer1_name, params));

    std::map<std::string, InputLayerDesc::Ptr> preprocessors =
        GetInputPreprocessors(image_inference, input_model_proc, &roi);
    MockInputBlob::Ptr input_blob = std::make_shared<MockInputBlob>();

    ASSERT_EQ(preprocessors[image_format]->name, layer1_name);
    ASSERT_TRUE(preprocessors[image_format]->preprocessor);
    ASSERT_NO_THROW(preprocessors[image_format]->preprocessor(input_blob));
    ASSERT_FALSE(preprocessors[image_format]->input_image_preroc_params);
}

TEST_F(CommonPreprocTest, MultipleInputModelLayers) {
    EXPECT_CALL(*image_inference, GetModelImageInputInfo(_, _, _, _, _)).Times(AtLeast(1));

    MockInputBlob::Ptr input_blob = std::make_shared<MockInputBlob>();
    std::vector<size_t> dims = {1, 4, 1, 2};
    float data[] = {1.0, 1.0, 1.0, 1.0};
    EXPECT_CALL(*input_blob, GetDims()).WillOnce(ReturnRef(dims));
    EXPECT_CALL(*input_blob, GetData()).WillOnce(Return(data));

    std::vector<ModelInputProcessorInfo::Ptr> input_model_proc;
    GstStructure *params1 = gst_structure_new("params", "resize", G_TYPE_STRING, "aspect-ratio", "crop", G_TYPE_STRING,
                                              "central", "color_space", G_TYPE_STRING, "RGB", NULL);
    input_model_proc.push_back(CreateModelInputProcessorInfo(layer1_name, params1));

    GstStructure *params2 = gst_structure_new("params", "scale", G_TYPE_DOUBLE, 2.0, NULL);
    input_model_proc.push_back(CreateModelInputProcessorInfo(layer2_name, params2, image_info_format));

    std::map<std::string, InputLayerDesc::Ptr> preprocessors =
        GetInputPreprocessors(image_inference, input_model_proc, &roi);
    ASSERT_EQ(preprocessors[image_format]->name, layer1_name);
    ASSERT_TRUE(preprocessors[image_format]->preprocessor);
    ASSERT_NO_THROW(preprocessors[image_format]->preprocessor(input_blob));

    InputImageLayerDesc::Ptr input_model_params = preprocessors[image_format]->input_image_preroc_params;
    ASSERT_TRUE(input_model_params->doNeedResize());
    ASSERT_TRUE(input_model_params->doNeedCrop());
    ASSERT_FALSE(input_model_params->doNeedColorSpaceConversion(InputImageLayerDesc::ColorSpace::RGB));
    ASSERT_FALSE(input_model_params->doNeedDistribNormalization());
    ASSERT_FALSE(input_model_params->doNeedRangeNormalization());

    ASSERT_EQ(preprocessors[image_info_format]->name, layer2_name);
    ASSERT_TRUE(preprocessors[image_info_format]->preprocessor);
    ASSERT_NO_THROW(preprocessors[image_info_format]->preprocessor(input_blob));
    ASSERT_FALSE(preprocessors[image_info_format]->input_image_preroc_params);
}

TEST_F(CommonPreprocTest, SequenceIndexInputModelLayerFormat) {
    std::vector<ModelInputProcessorInfo::Ptr> input_model_proc;
    GstStructure *params = gst_structure_new_empty("params");
    const std::string sequence_index_img_format = "sequence_index";
    input_model_proc.push_back(CreateModelInputProcessorInfo(layer1_name, params, sequence_index_img_format));

    std::map<std::string, InputLayerDesc::Ptr> preprocessors =
        GetInputPreprocessors(image_inference, input_model_proc, &roi);

    MockInputBlob::Ptr input_blob = std::make_shared<MockInputBlob>();
    std::vector<size_t> dims = {3, 2, 1, 0};
    float data[] = {3.0, 2.0, 1.0};
    EXPECT_CALL(*input_blob, GetDims()).WillOnce(ReturnRef(dims));
    EXPECT_CALL(*input_blob, GetData()).WillOnce(Return(data));

    ASSERT_EQ(preprocessors[sequence_index_img_format]->name, layer1_name);
    ASSERT_TRUE(preprocessors[sequence_index_img_format]->preprocessor);
    ASSERT_NO_THROW(preprocessors[sequence_index_img_format]->preprocessor(input_blob));
    ASSERT_FALSE(preprocessors[sequence_index_img_format]->input_image_preroc_params);
}

TEST_F(CommonPreprocTest, FaceAlignmentInputModelLayerFormat) {
    float landmarks[] = {0.1, 0.2, 0.1, 0.2, 0.1, 0.2, 0.1, 0.2, 0.1, 0.2};
    GVariant *v = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, landmarks, sizeof(landmarks), 1);
    gsize n_elem = 0;
    GstStructure *gst_landmarks =
        gst_structure_new("landmarks", "format", G_TYPE_STRING, "landmark_points", "data_buffer", G_TYPE_VARIANT, v,
                          "data", G_TYPE_POINTER, g_variant_get_fixed_array(v, &n_elem, 1), NULL);
    GstVideoRegionOfInterestMeta landmarks_roi = GstVideoRegionOfInterestMeta();
    landmarks_roi.params = nullptr;
    landmarks_roi.params = g_list_append(landmarks_roi.params, gst_landmarks);

    std::vector<ModelInputProcessorInfo::Ptr> input_model_proc;
    std::vector<double> alignment_points = {
        0.31556875000000000, 0.4615741071428571,  0.68262291666666670, 0.4615741071428571,  0.50026249999999990,
        0.6405053571428571,  0.34947187500000004, 0.8246919642857142,  0.65343645833333330, 0.8246919642857142};
    GValueArray *arr = ConvertVectorToGValueArr(alignment_points);
    GstStructure *params = gst_structure_new_empty("params");
    gst_structure_set_array(params, "alignment_points", arr);

    input_model_proc.push_back(CreateModelInputProcessorInfo(layer1_name, params));
    std::map<std::string, InputLayerDesc::Ptr> preprocessors =
        GetInputPreprocessors(image_inference, input_model_proc, &landmarks_roi);

    MockInputBlob::Ptr input_blob = std::make_shared<MockInputBlob>();
    std::vector<size_t> dims = {1, 1, 1, 1};
    float data[] = {3.0, 2.0, 1.0};
    EXPECT_CALL(*input_blob, GetDims()).WillOnce(ReturnRef(dims));
    EXPECT_CALL(*input_blob, GetLayout()).WillOnce(Return(InputBlob::Layout::NCHW));
    EXPECT_CALL(*input_blob, GetData()).WillOnce(Return(data));
    EXPECT_CALL(*input_blob, GetIndexInBatch()).WillOnce(Return(0));

    ASSERT_EQ(preprocessors[image_format]->name, layer1_name);
    ASSERT_TRUE(preprocessors[image_format]->preprocessor);
    ASSERT_NO_THROW(preprocessors[image_format]->preprocessor(input_blob));
    ASSERT_TRUE(preprocessors[image_format]->input_image_preroc_params);
}
