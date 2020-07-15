/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __AUDIO_UTILS__
#define __AUDIO_UTILS__

#ifdef __cplusplus
extern "C" {
#endif

#define SAMPLE_AUDIO_RATE 16000
#define MULTIPLIER 62500 // NANO_SECOND/SAMPLE_AUDIO_RATE
#define AUDIO_CAPS "audio/x-raw,format=S16LE,rate=16000,channels=1,layout=interleaved"
#define MAX_PROC_FILE_SIZE 4194304     // In bytes, 4MB
#define MAX_MODEL_FILE_SIZE 1000000000 // In bytes, 1 GB

#ifdef __cplusplus
}

#endif

#endif /* __AUDIO_UTILS__ */