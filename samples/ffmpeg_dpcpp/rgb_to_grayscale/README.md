# Sample

The sample implements pipeline
```
FFmpeg input -> FFmpeg VAAPI decode -> VAAPI resize -> SYCL kernel
```

Deep Learning Streamer (DL Streamer) C++ element `ffmpeg_multi_source` is used for first 3 stages of pipeline.

The SYCL kernel is simple kernel converting RGB into Grayscale. It requires Intel® oneAPI Level Zero support to work.

## Build and Run

The sample requires Intel® oneAPI/DPC++ environment to be enabled via
```
source /opt/intel/oneapi/setvars.sh
```

If you are building dlstreamer from source, add "-DENABLE_SAMPLES=ON" to the cmake command line when you are building dlstreamer, and run the resulting ffmpeg_dpcpp_rgb_to_grayscale binary as:
```
./intel64/Debug/bin/ffmpeg_dpcpp_rgb_to_grayscale -i your_video.mp4 -o test.gray
```

If you are building stand alone samples with dlstreamer installed from .deb packages or as a docker image, you can build and run it using

```sh
./build_and_run.sh INPUT_VIDEO
```

The sample outputs file `output.gray` with Grayscale images which could viewed by `ffplay` command
```
ffplay -f rawvideo -pix_fmt gray -s 640x480 -i ~/intel/dl_streamer/samples/ffmpeg_dpcpp_rgb_to_grayscale/build/output.gray
```
