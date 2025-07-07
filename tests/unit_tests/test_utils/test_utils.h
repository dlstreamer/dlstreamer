/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { EXIT_STATUS_SUCCESS = 0, EXIT_STATUS_FAILURE = 1 } ExitStatus;

#define MAX_STR_PATH_SIZE 1024

ExitStatus file_exists(const char *file_name);
ExitStatus get_model_path(char *model_path, size_t model_path_size, const char *model_name, const char *fp);
ExitStatus get_model_proc_path(char *model_proc_path, size_t model_proc_path_size, const char *filename_no_ext);
ExitStatus get_video_file_path(char *video_file_path, size_t video_path_size, const char *filename);

#define GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buf, state)                                                          \
    ((GstVideoRegionOfInterestMeta *)gst_buffer_iterate_meta_filtered(buf, state,                                      \
                                                                      GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE))

#ifdef __cplusplus
}
#endif
