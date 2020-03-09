# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

import sys
import gi
gi.require_version('Gst', '1.0')
from gi.repository import Gst, GObject

from gstgva import VideoFrame

DETECT_THRESHOLD = 0.5

Gst.init(sys.argv)

REGION_TENSOR = VideoFrame.create_labels_structure(["background", "face", "my_label_2", "etc."])

def process_frame(frame: VideoFrame, threshold: float = DETECT_THRESHOLD) -> bool:
    width = frame.video_info().width
    height = frame.video_info().height    

    for tensor in frame.tensors():
        dims = tensor.dims()
        data = tensor.data()
        object_size = dims[-1]
        for i in range(dims[-2]):
            image_id = data[i * object_size + 0]
            confidence = data[i * object_size + 2]
            x_min = int(data[i * object_size + 3] * width + 0.5)
            y_min = int(data[i * object_size + 4] * height + 0.5)
            x_max = int(data[i * object_size + 5] * width + 0.5)
            y_max = int(data[i * object_size + 6] * height + 0.5)

            if image_id != 0:
                break
            if confidence < threshold:
                continue

            frame.add_region(x_min, y_min, x_max - x_min, y_max - y_min, 1, region_tensor=REGION_TENSOR)

    return True
