# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
TEMPLATE_SUFFIX = ".template"
GT_BASE_FOLDER_FIELD = "dataset.groundtruth.base"
GT_FILE_NAME_FIELD = "dataset.groundtruth"
GT_FILE_NAME_TEMPLATE = GT_FILE_NAME_FIELD + TEMPLATE_SUFFIX
TEST_SET_PROPS_FIELD = "test_set_properties"
ARTIFACTS_PATH_FIELD = "dataset.artifacts"

TEST_SETS_FIELD = "test_sets"
COMPARATOR_FIELD = "dataset.comparator_type"
ATTACHROI_DIR_FIELD = "dataset.attachroi_json.dir"
PIPELINE_CMD_FIELD = "pipeline.cmd"
PIPELINE_TEMPLATE_FIELD = "pipeline" + TEMPLATE_SUFFIX
PREDICTION_PATH_FIELD = "prediction_path"
TC_NAME_FIELD = "test_case.name"
TC_NAME_TEMPLATE_FIELD = TC_NAME_FIELD + TEMPLATE_SUFFIX
EXE_DIR = "exe.dir"
EXE_DIR_TEMPLATE = EXE_DIR + TEMPLATE_SUFFIX
OUT_VIDEO = "pipeline.out_video_name"
OUT_VIDEO_TEMPLATE = OUT_VIDEO + TEMPLATE_SUFFIX
TEST_TYPE = "test.type"
SAMPLE_DIR = "sample.dir"
VIDEO_DIR = "dataset.video"
SAMPLE_COMMAND_TEMPLATE = "sample.command"
BENCHMARK_PERFORMANCE_COMMAND_TEMPLATE="benchmark_performance.command"
BENCHMARK_PERFORMANCE_FPS_FPI = "dataset.target_fps_kpi"
BENCHMARK_PERFORMANCE_FPS_PASS_THRESHOLD = "dataset.percentage_fps_pass_threshold"