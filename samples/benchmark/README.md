# Benchmark Sample

This sample demonstrates [gvafpscounter](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvafpscounter) element used to measure overall performance of single-channel or multi-channel video analytics pipelines.

The sample outputs FPS (Frames Per Second) every second and average FPS on exit.

## How It Works
The sample builds GStreamer pipeline containing video decode, inference and other IO elements, or multiple (N) identical pipelines if number channels parameter set to N>1.

The `gvafpscounter` inserted at the end of each channel pipeline and measures FPS across all channels.

The command-line parameters allow to select decode and inference devices (ex, CPU, GPU).

## Models

By default the sample measures performance of video analytics pipeline on `person-vehicle-bike-detection-crossroad-0078` model.

Modify `MODEL=` line in the script to benchmark pipeline on another model.

> **NOTE**: Before running samples (including this one), run script `download_models.sh` once (the script located in `samples` top folder) to download all models required for this and other samples.

## Input video

You can download video file example by command
```sh
curl https://github.com/intel-iot-devkit/sample-videos/raw/master/head-pose-face-detection-female-and-male.mp4\" --output /path/to/your/video/head-pose-face-detection-female-and-male.mp4
```
or use any other media/video file.

## Running

```sh
./benchmark.sh INPUT_VIDEO [DECODE_DEVICE] [INFERENCE_DEVICE] [CHANNELS_COUNT]
```

The sample takes one to four command-line parameters (last three are optional):
1. [INPUT_VIDEO] to specify input video file
2. [DECODE_DEVICE] to specify device for video decode, could be
    * CPU (Default)
    * GPU
3. [INFERENCE_DEVICE] to specify inference device, could be any device supported by OpenVINO™ Toolkit
    * CPU (Default)
    * GPU
    * HDDL
    * ...
4. [CHANNELS_COUNT] number simultaneous channels to benchmark

## Sample Output

The sample
* prints gst-launch command line into console
* reports FPS every second and average FPS on exit

## See also
* [DL Streamer samples](../README.md)
