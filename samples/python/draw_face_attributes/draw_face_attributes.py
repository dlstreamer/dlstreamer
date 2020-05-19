# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

from gstgva import VideoFrame, util
import sys
import numpy
import cv2
from argparse import ArgumentParser

import gi
gi.require_version('GObject', '2.0')
gi.require_version('Gst', '1.0')
gi.require_version('GstApp', '1.0')
gi.require_version('GstVideo', '1.0')
from gi.repository import Gst, GLib, GstApp, GstVideo

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


def frame_callback(frame: VideoFrame):
    with frame.data() as mat:
        for roi in frame.regions():
            labels = []
            rect = roi.rect()
            for tensor in roi.tensors():
                data = tensor.data()
                if "align_fc3" == tensor.layer_name():
                    lm_color = (255, 0, 0)
                    for i in range(0, len(data), 2):
                        x = int(rect.x + rect.w * data[i])
                        y = int(rect.y + rect.h * data[i + 1])
                        cv2.circle(mat, (x, y), int(
                            1 + 0.02 * rect.w), lm_color, -1)
                if "prob" == tensor.layer_name():
                    if data[1] > 0.5:
                        labels.append("M")
                    else:
                        labels.append("F")
                if "age_conv3" == tensor.layer_name():
                    labels.append(str(int(data[0] * 100)))
                if "prob_emotion" == tensor.layer_name():
                    emotions = ["neutral", "happy", "sad", "surprise", "anger"]
                    index = numpy.argmax(data)
                    labels.append(emotions[index])

            if labels:
                label = " ".join(labels)
                cv2.putText(mat, label, (rect.x, rect.y + rect.h + 30),
                            cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)


def pad_probe_callback(pad, info):
    with util.GST_PAD_PROBE_INFO_BUFFER(info) as buffer:
        caps = pad.get_current_caps()
        frame = VideoFrame(buffer, caps=caps)
        frame_callback(frame)

    return Gst.PadProbeReturn.OK


def create_launch_string():
    if "/dev/video" in args.input:
        source = "v4l2src device"
    elif "://" in args.input:
        source = "urisourcebin buffer-size=4096 uri"
    else:
        source = "filesrc location"

    return "{}={} ! decodebin ! \
    videoconvert n-threads=4 ! capsfilter caps=\"video/x-raw,format=BGRx\" ! \
    gvadetect model={} device=CPU batch-size=1 ! queue ! \
    gvaclassify model={} device=CPU batch-size=1 ! queue ! \
    gvaclassify model={} device=CPU batch-size=1 ! queue ! \
    gvaclassify model={} batch-size=1 ! queue ! \
    gvawatermark name=gvawatermark ! videoconvert n-threads=4 ! \
    fpsdisplaysink video-sink=xvimagesink sync=false".format(source, args.input, args.detection_model,
                                                             args.classification_model1, args.classification_model2,
                                                             args.classification_model3)


def glib_mainloop():
    mainloop = GLib.MainLoop()
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
        sys.exit()
    else:
        pass
    return True


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

    glib_mainloop()

    print("Exiting")
