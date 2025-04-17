Download Public Models
======================


This page provides instructions on how to use the *samples/download_public_models.sh* script to download 

   - `YOLO <https://docs.ultralytics.com/models/>`__
   - `CenterFace <https://github.com/Star-Clouds/CenterFace>`__
   - `HSEmotion <https://github.com/av-savchenko/face-emotion-recognition>`__
   - `Deeplabv3 <https://github.com/openvinotoolkit/open_model_zoo/blob/master/models/public/deeplabv3/README.md#deeplabv3>`__  models.

The script downloads the models from the respective sources, handles the necessary conversions, and places the model files in a directory specified by the MODELS_PATH environment variable.

Link to check `supported models <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/download_public_models.sh#L12>`__


Example for downloading YOLOv11s model:


.. code-block:: none

    export MODELS_PATH=/path/to/models

    ./samples/download_public_models.sh yolo11s



