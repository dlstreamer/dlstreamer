# ==============================================================================
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

import unittest
import os

from pipeline_runner import TestPipelineRunner
from utils import *

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
IMAGE_PATH = os.path.join(SCRIPT_DIR, "test_files", "face_detection.png")

d_model_name = "face-detection-adas-0001"
d_model_path = get_model_path(d_model_name)
c_model_name = "emotions-recognition-retail-0003"
c_model_path = get_model_path(c_model_name)
c_model_proc_name = "emotions-recognition-retail-0003"
c_model_proc_path = get_model_proc_path(c_model_proc_name)


PIPELINE_STR = """appsrc name=mysrc \
! decodebin ! videoconvert ! video/x-raw,format=BGRA \
! gvadetect model={} ! queue \
! gvaclassify model={} model-proc={} pre-process-backend=opencv \
! gvawatermark \
! appsink name=mysink emit-signals=true sync=false """.format(d_model_path, c_model_path, c_model_proc_path)

GOLD_TRUE = [
    BBox(0.6924214363098145, 0.1818782240152359, 0.8225287199020386, 0.5060485601425171,
         [
             {
                 'label': "surprise",
                 'layer_name': "Plus692_Output_0",
                 'name': "ANY"
             }
         ], class_id=1
         ),
    BBox(0.18316438794136047, 0.19858478009700775, 0.3025963306427002, 0.5096352100372314,
         [
             {
                 'label': "happiness",
                 'layer_name': "Plus692_Output_0",
                 'name': "ANY"
             }
         ], class_id=1
         )
]


class TestDetectionClassificationONNXPipeline(unittest.TestCase):
    def test_detection_classification_onnx_pipeline(self):
        pipeline_runner = TestPipelineRunner()
        pipeline_runner.set_pipeline(PIPELINE_STR,
                                     IMAGE_PATH,
                                     GOLD_TRUE)
        pipeline_runner.run_pipeline()
        for e in pipeline_runner.exceptions:
            print(e)
        pipeline_runner.assertEqual(len(pipeline_runner.exceptions), 0,
                                    "Exceptions have been caught.")


if __name__ == "__main__":
    unittest.main()
