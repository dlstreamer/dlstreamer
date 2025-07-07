# ==============================================================================
# Copyright (C) 2023-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

import unittest
import os

from pipeline_runner import TestPipelineRunner
from utils import *

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
IMAGE_PATH = os.path.join(SCRIPT_DIR, "test_files",
                          "10_bottle_with_barcodes.jpg")

D_MODEL_NAME = "yolo11s"
D_MODEL_PATH = get_model_path(D_MODEL_NAME)


PIPELINE_STR = """appsrc name=mysrc \
! decodebin ! videoconvert ! video/x-raw,format=BGRA \
! gvadetect pre-process-backend=ie model={} \
! opencv_barcode_detector \
! appsink name=mysink emit-signals=true sync=false """.format(D_MODEL_PATH)


GOLD_TRUE = [
    BBox(0.6161232384461646, 0.03477298073132307,
         0.7025184438007837, 0.4223918858526927, [], class_id=39),
    BBox(0.5129911171345578, 0.034421420610270204,
         0.5988621175119595, 0.421146136837395, [], class_id=39),
    BBox(0.40945051624552176, 0.031162167059811452,
         0.49448769531380665, 0.42071467072836555, [], class_id=39),
    BBox(0.209264576573142, 0.03087478922120912,
         0.2934395744419034, 0.4183383884778378, [], class_id=39),
    BBox(0.21635495448641406, 0.5214243608925395,
         0.30052995235517543, 0.9068217770281697, [], class_id=39),
    BBox(0.30892719192694407, 0.02384051120043562,
         0.39299914528428226, 0.41327740569826066, [], class_id=39),
    BBox(0.617521238588113, 0.5172322588746585,
         0.7033573821687307, 0.903723229523349, [], class_id=39),
    BBox(0.3154697495023868, 0.5155178862142247,
         0.4026027709633524, 0.9031755820389922, [], class_id=39),
    BBox(0.517076505975075, 0.5126001434415599,
         0.6037522346340451, 0.8969782962944848, [], class_id=39),
    BBox(0.418180001052916, 0.5193119841711518,
         0.5048557297118861, 0.9002123251588987, [], class_id=39),
    BBox(0.6231770833333333, 0.24259259259259258,
         0.7052083333333333, 0.3013888888888889, [], class_id=None),
    BBox(0.51875, 0.24444444444444444,
         0.60078125, 0.30462962962962964, [], class_id=None),
    BBox(0.41328125, 0.2439814814814815,
         0.4953125, 0.3050925925925926, [], class_id=None),
    BBox(0.21302083333333333, 0.23981481481481481,
         0.2947916666666667, 0.300462962962963, [], class_id=None),
    BBox(0.22161458333333334, 0.7277777777777777,
         0.303125, 0.7870370370370371, [], class_id=None),
    BBox(0.3125, 0.23657407407407408,
         0.39348958333333334, 0.29583333333333334, [], class_id=None),
    BBox(0.6221354166666667, 0.7231481481481481,
         0.70390625, 0.7828703703703703, [], class_id=None),
    BBox(0.32005208333333335, 0.7217592592592592,
         0.40130208333333334, 0.7824074074074074, [], class_id=None),
    BBox(0.5213541666666667, 0.7171296296296297,
         0.6033854166666667, 0.7763888888888889, [], class_id=None),
    BBox(0.42213541666666665, 0.7231481481481481,
         0.503125, 0.7819444444444444, [], class_id=None)
]


class TestBarcodeDetectorPipeline(unittest.TestCase):
    def test_detection_yolo_v3_pipeline(self):
        pipeline_runner = TestPipelineRunner()
        pipeline_runner.set_pipeline(PIPELINE_STR, IMAGE_PATH, GOLD_TRUE)
        pipeline_runner.run_pipeline()
        for e in pipeline_runner.exceptions:
            print(e)
        pipeline_runner.assertEqual(len(pipeline_runner.exceptions), 0,
                                    "Exceptions have been caught.")


if __name__ == "__main__":
    unittest.main()
