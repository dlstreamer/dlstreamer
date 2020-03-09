# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

from gstgva import VideoFrame


def process_frame(frame: VideoFrame) -> bool:
    for roi in frame.regions():
        for tensor in roi:
            if tensor.name() == 'detection':
                continue
            layer_name = tensor.layer_name()
            data = tensor.data()
            if 'age' in layer_name:
                tensor.set_label(str(int(data[0] * 100)))
            if 'gender' in tensor.model_name() and 'prob' in layer_name:
                tensor.set_label(" M " if data[1] > 0.5 else " F ")
            if 'EmoNet' in layer_name:
                emotions = ["neutral", "happy", "sad", "surprise", "anger"]
                tensor.set_label(emotions[data.index(max(data))])

    return True
