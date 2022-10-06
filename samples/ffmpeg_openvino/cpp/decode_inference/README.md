# Sample

The sample implements basic pipeline
```
FFmpeg input -> FFmpeg VAAPI decode -> OpenVINO inference
```
directly calling FFmpeg/libav and OpenVINO interfaces, without any dependency on Intel® DL Streamer.
This sample demonstrates zero-copy buffer sharing FFmpeg->VAAPI->OpenVINO, but it's not
optimized for performance. For more optimized version with multi-stream and batching support
please refer to sample `decode_resize_inference`.

## Build and Run

Use cmake build system or run the following script

```sh
./build_and_run.sh INPUT_VIDEO [MODEL]
```
