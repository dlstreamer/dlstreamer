# Sample

The sample implements pipeline
```
FFmpeg input -> FFmpeg VAAPI decode -> VAAPI resize -> OpenVINO inference
```
with the following performance optimizations for GPU
1. multi-stream input and decode
2. async inference (multiple inference requests)
3. batched inference

Intel® DL Streamer C++ element `ffmpeg_multi_source` is used for first 3 stages of pipeline, the inference
stage implemented by directly calling OpenVINO API 2.0.  

## Build and Run

Use cmake build system or run the following script

```sh
./build_and_run.sh INPUT_VIDEO [MODEL]
```
