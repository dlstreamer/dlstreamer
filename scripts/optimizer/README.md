# Overview
DLSOptimizer is a tool for helping users discover more optimal versions of the pipelines they run on DL Streamer. It will explore different modifications to your pipeline that are known to increase performance and measure it. As a result, you can expect to receive a pipeline that's better suited to your setup.

# Limitations
Currently the DLSOptimizer focuses mainly on DL Streamers elements, specifically the `gvadetect` and `gvaclassify` elements. The produced pipeline could still have potential for further optimization by transforming other elements.

Multi-stream pipelines are also currently not supported.

# Usage
```
python3 optimizer.py [OPT] -- PIPELINE

Options:
    --search-duration SEARCH_DURATION   How long should the optimizer search for better pipelines
    --sample-duration SAMPLE_DURATION   How long should every pipeline be sampled for performance
```

Increasing the search duration will increase the chances of discovering more performant pipelines. Increasing the sample duration will improve the stability of the search, but less pipelines will potentially be explored. 

# Example
```
 python3 optimizer.py -- urisourcebin buffer-size=4096 uri=https://videos.pexels.com/video-files/1192116/1192116-sd_640_360_30fps.mp4 ! decodebin ! gvadetect model=/home/optimizer/models/public/yolo11s/INT8/yolo11s.xml ! queue ! gvawatermark ! vah264enc ! h264parse ! mp4mux ! fakesink
[__main__] [    INFO] - GStreamer initialized successfully
[__main__] [    INFO] - GStreamer version: 1.26.6
[__main__] [    INFO] - Detected GPU Device
[__main__] [    INFO] - No NPU Device detected
[__main__] [    INFO] - Sampling for 10 seconds...
FpsCounter(last 1.00sec): total=46.87 fps, number-streams=1, per-stream=46.87 fps
FpsCounter(average 1.00sec): total=46.87 fps, number-streams=1, per-stream=46.87 fps
FpsCounter(last 1.01sec): total=43.70 fps, number-streams=1, per-stream=43.70 fps
FpsCounter(average 2.01sec): total=45.28 fps, number-streams=1, per-stream=45.28 fps

...

FpsCounter(last 1.09sec): total=73.45 fps, number-streams=1, per-stream=73.45 fps
FpsCounter(average 8.70sec): total=73.65 fps, number-streams=1, per-stream=73.65 fps
[__main__] [    INFO] - Best found pipeline: urisourcebin buffer-size=4096 uri=https://videos.pexels.com/video-files/1192116/1192116-sd_640_360_30fps.mp4 !decodebin3!gvadetect model=/home/optimizer/models/public/yolo11s/INT8/yolo11s.xml device=GPU pre-process-backend=va-surface-sharing batch-size=2 nireq=2 ! queue ! gvawatermark ! vah264enc ! h264parse ! mp4mux ! fakesink with fps: 81.987923.2
```
In this case the optimizer started with a pipeline that ran at ~45fps, and found a pipeline that ran at ~82fps instead. The specific improvements were:
 - replacing the `decodebin` with the `decodebin3` element.
 - configuring the `gvadetect` element to use GPU for processing
 - setting the `batch-size` parameter to 2
 - setting the `nireq` parameter to 2