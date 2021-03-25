# Benchmark Sample

This sample demonstrates [gvafpscounter](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvafpscounter) element used to measure overall performance of single-channel or multi-channel video analytics pipelines.

The sample outputs FPS (Frames Per Second) every second and average FPS on exit.

## How It Works
The sample builds GStreamer pipeline containing video decode, inference and other IO elements, or multiple (N) identical pipelines if number channels parameter set to N>1.

The `gvafpscounter` inserted at the end of each channel pipeline and measures FPS across all channels.

## Models

By default the sample measures performance of video analytics pipeline on `person-vehicle-bike-detection-crossroad-0078` model.

Modify `MODEL=` line in the script to benchmark pipeline on another model.

> **NOTE**: Before running samples (including this one), run script `download_models.bat` once (the script located in `win_samples` top folder) to download all models required for this and other samples.

## Input video

You can download video file example by command
```bat
powershell.exe -command "Invoke-WebRequest https://github.com/intel-iot-devkit/sample-videos/raw/master/bolt-detection.mp4 -OutFile /path/to/bolt-detection.mp4" 
```
or use any other media/video file.

## Running

```bat
benchmark.bat INPUT_VIDEO [INFERENCE_DEVICE] [CHANNELS_COUNT]
```

The sample takes one to four command-line parameters (last three are optional):
1. [INPUT_VIDEO] to specify input video file
2. [INFERENCE_DEVICE] to specify inference device, could be any device supported by OpenVINO™ Toolkit
    * CPU (Default)
    * GPU
    * HDDL
    * ...
3. [CHANNELS_COUNT] number simultaneous channels to benchmark

## Sample Output

The sample
* prints gst-launch command line into console
* reports FPS every second and average FPS on exit

## See also
* [DL Streamer samples](..\README.md)
