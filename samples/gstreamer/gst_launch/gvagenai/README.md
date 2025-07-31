# MiniCPM with gvagenai Element

This directory contains a script demonstrating how to use the `gvagenai` element with MiniCPM-V 2.6 for video summerization.

The `gvagenai` element integrates OpenVINO™ GenAI capabilities into video processing pipelines. It supports visual language models like MiniCPM-V for video content description and analysis.

## How It Works

The script constructs a GStreamer pipeline that processes video input from various sources (file, URL, or camera) and applies the MiniCPM-V model for generating summerization of the video content.

The sample utilizes GStreamer command-line tool `gst-launch-1.0` which can build and run a GStreamer pipeline described in a string format.
The string contains a list of GStreamer elements separated by an exclamation mark `!`, each element may have properties specified in the format `property=value`.

This sample builds GStreamer pipeline of the following elements:
- `filesrc` or `urisourcebin` or `v4l2src` for input from file/URL/web-camera
- `decodebin3` for video decoding
- `videoconvert` for converting video frame into RGB color format
- `gvagenai` for inferencing with MiniCPM-V model to generate text descriptions
- `gvametapublish` for saving inference results to a JSON file
- `fakesink` for discarding output

## MiniCPM-V Model Preparation

You need to prepare your MiniCPM-V model in OpenVINO™ format, you can learn more from [Visual-language assistant with MiniCPM-V2 and OpenVINO™](https://github.com/openvinotoolkit/openvino_notebooks/blob/latest/notebooks/minicpm-v-multimodal-chatbot/minicpm-v-multimodal-chatbot.ipynb):

```bash
optimum-cli export openvino --model openbmb/MiniCPM-V-2_6 --weight-format int4 MiniCPM-V-2_6
```

Set the model path:

```bash
export MINICPM_MODEL_PATH=/path/to/your/MiniCPM-V-2_6
```

## Running the Sample

**Usage:**
```bash
./minicpm_with_gvagenai.sh [OPTIONS]
```

**Options:**
- `-s, --source FILE/URL/CAMERA`: Input source (file path, URL or web camera)
- `-d, --device DEVICE`: Inference device (CPU, GPU, NPU)
- `-p, --prompt TEXT`: Text prompt for the model
- `-r, --frame-rate RATE`: Frame sampling rate (fps)
- `-c, --chunk-size NUM`: Chunk size, or frames per inference call
- `-t, --max-tokens NUM`: Maximum new tokens to generate
- `-m, --metrics`: Include performance metrics in JSON output
- `-h, --help`: Show help message

**Examples:**

1. **Basic usage with default settings**
   ```bash
   ./minicpm_with_gvagenai.sh
   ```

2. **Custom settings example**
   ```bash
   ./minicpm_with_gvagenai.sh --source /path/to/video.mp4 --device GPU --prompt "Describe what do you see in this video?" --chunk-size 10 --frame-rate 1 --max-tokens 100
   ```

3. **With performance metrics enabled**
   ```bash
   ./minicpm_with_gvagenai.sh --metrics --max-tokens 200
   ```

4. **Print more logs**
   ```bash
   GST_DEBUG=gvagenai:4 ./minicpm_with_gvagenai.sh
   ```

**Output:**
- Results are saved to `minicpm_output.json`
- Contains inference results with timestamps and metadata
- When `--metrics` is enabled, includes performance metrics such as inference time and throughput

## See also
* [Samples overview](../../README.md)
