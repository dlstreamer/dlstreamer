# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

import sys
import numpy
import cv2
from argparse import ArgumentParser

import gi

gi.require_version('GObject', '2.0')
gi.require_version('Gst', '1.0')
gi.require_version('GstApp', '1.0')
gi.require_version('GstVideo', '1.0')

from gi.repository import Gst, GObject, GstApp, GstVideo
from gstgva import VideoFrame, util

parser = ArgumentParser(add_help=False)
_args = parser.add_argument_group('Options')
_args.add_argument("-i", "--input", help="Required. Path to input video file",
                   required=True, type=str)
_args.add_argument("-d", "--detection_model", help="Required. Path to an .xml file with object detection model",
                   required=True, type=str)
_args.add_argument("-c1", "--classification_model1",
                   help="Required. Path to an .xml file with object classification model",
                   required=True, type=str)
_args.add_argument("-c2", "--classification_model2",
                   help="Required. Path to an .xml file with object classification model",
                   required=True, type=str)
_args.add_argument("-c3", "--classification_model3",
                   help="Required. Path to an .xml file with object classification model",
                   required=True, type=str)
args = parser.parse_args()


def create_launch_string():
    return "filesrc location={} ! qtdemux ! avdec_h264 ! \
    videoconvert n-threads=4 ! capsfilter caps=\"video/x-raw,format=BGRx\" ! \
    gvadetect model={} device=CPU batch-size=1 ! queue ! \
    gvaclassify model={} device=CPU batch-size=1 ! queue ! \
    gvaclassify model={} device=CPU batch-size=1 ! queue ! \
    gvaclassify model={} batch-size=1 ! queue ! \
    gvawatermark name=gvawatermark ! videoconvert n-threads=4 ! \
    fpsdisplaysink video-sink=xvimagesink sync=false".format(args.input, args.detection_model,
                                                             args.classification_model1, args.classification_model2,
                                                             args.classification_model3)


def gobject_mainloop():
    mainloop = GObject.MainLoop()
    try:
        mainloop.run()
    except KeyboardInterrupt:
        pass


def bus_call(bus, message, pipeline):
    t = message.type
    if t == Gst.MessageType.EOS:
        print("pipeline ended")
        pipeline.set_state(Gst.State.NULL)
        sys.exit()
    elif t == Gst.MessageType.ERROR:
        print("error {}".format(message))
    else:
        pass
    return True


def frame_callback(frame: VideoFrame):
    with frame.data() as mat:
        for roi in frame.regions():
            labels = []
            for tensor in roi:
                data = tensor.data()
                if "landmarks" in tensor.model_name():
                    lm_color = (255, 0, 0)
                    for i in range(0, len(data), 2):
                        x = int(roi.meta().x + roi.meta().w * data[i])
                        y = int(roi.meta().y + roi.meta().h * data[i + 1])
                        cv2.circle(mat, (x, y), int(1 + 0.02 * roi.meta().w), lm_color, -1)
                if "gender" in tensor.model_name() and "prob" in tensor.layer_name():
                    if data[1] > 0.5:
                        labels.append("M")
                    else:
                        labels.append("F")
                elif "age" in tensor.layer_name():
                    labels.append(str(int(data[0] * 100)))
                elif "EmoNet" in tensor.model_name():
                    emotions = ["neutral", "happy", "sad", "surprise", "anger"]
                    index = numpy.argmax(data)
                    labels.append(emotions[index])

            if labels:
                label = " ".join(labels)
                cv2.putText(mat, label, (roi.meta().x, roi.meta().y + roi.meta().h + 30), cv2.FONT_HERSHEY_SIMPLEX,
                            1, (0, 0, 255), 2)


def pad_probe_callback(pad, info):
    with util.GST_PAD_PROBE_INFO_BUFFER(info) as buffer:
        caps = pad.get_current_caps()
        frame = VideoFrame(buffer, caps=caps)
        frame_callback(frame)

    return Gst.PadProbeReturn.OK


def set_callbacks(pipeline):
    gvawatermark = pipeline.get_by_name("gvawatermark")
    pad = gvawatermark.get_static_pad("src")
    pad.add_probe(Gst.PadProbeType.BUFFER, pad_probe_callback)

    bus = pipeline.get_bus()
    bus.add_signal_watch()
    bus.connect("message", bus_call, pipeline)


if __name__ == '__main__':
    Gst.init(sys.argv)
    gst_launch_string = create_launch_string()
    print(gst_launch_string)
    pipeline = Gst.parse_launch(gst_launch_string)

    set_callbacks(pipeline)

    pipeline.set_state(Gst.State.PLAYING)

    gobject_mainloop()

    print("Exiting")
