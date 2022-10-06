# Sample

The sample implements pipeline
```
FFmpeg input -> FFmpeg VAAPI decode -> VAAPI resize -> SYCL kernel
```

Intel® DL Streamer C++ element `ffmpeg_multi_source` is used for first 3 stages of pipeline.

The SYCL kernel is simple kernel converting RGB into Grayscale.

## Build and Run

The sample requires oneAPI/DPC++ environment enabled
```
source /opt/intel/oneapi/setvars.sh
```

Use cmake build system or run the following script

```sh
./build_and_run.sh INPUT_VIDEO [MODEL]
```

The sample outputs file `output.gray` with Grayscale images which could viewed by `ffplay` command
```
ffplay -f rawvideo -pix_fmt gray -s 640x480 -i ~/intel/dl_streamer/samples/ffmpeg_dpcpp_rgb_to_grayscale/build/output.gray
```
