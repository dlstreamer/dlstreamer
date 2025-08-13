# Download Public Models

This page provides instructions on how to use the
*samples/download_public_models.sh* script to download

- [YOLO](https://docs.ultralytics.com/models/)
- [CenterFace](https://github.com/Star-Clouds/CenterFace)
- [HSEmotion](https://github.com/av-savchenko/face-emotion-recognition)
- [Deeplabv3](https://github.com/openvinotoolkit/open_model_zoo/blob/master/models/public/deeplabv3/README.md#deeplabv3)
  models.

The script downloads the models from the respective sources, handles the
necessary conversions, and places the model files in a directory
specified by the MODELS_PATH environment variable.

Link to check
[supported models](https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/download_public_models.sh#L12)

Example for downloading YOLOv11s model:

``` none
export MODELS_PATH=/path/to/models

./samples/download_public_models.sh yolo11s
```

## Quantization

You can perform INT8 quantization on some of the models by specifying a
second parameter with a dataset to be used for the quantization process.

``` none
./samples/download_public_models.sh yolo11s coco128
```

Currently available datasets are `coco` and `coco128`.

> **NOTE:** `coco` is a very large dataset of over 20GB and containing more than a
> 100,000 images. Quantization on this dataset can take a very long time.
> For development purposes, it is recommended to use `coco128` instead
> which is much lighter.

Models which currently support quantization are:

- YOLOv5: nu, su, mu, lu, xu, n6u, s6u, m6u, l6u, x6u
- YOLOv8: n, s, m, l, x
- YOLOV9: t, s, m, c, e
- YOLOv10: n, s, m, b, l, x
- YOLOv11: n, s, m, l, x
