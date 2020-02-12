#!/usr/bin/python3
# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

import subprocess
import argparse
import signal
import os
import re

arg_parser = argparse.ArgumentParser()
arg_parser.add_argument('video_path', default='', type=str,
                        help='specify the path to the video file')
arg_parser.add_argument('-g', '--set_gui', dest='set_gui', action='store_true',
                        help='play video in a graphics window')
arg_parser.add_argument('--num_channels', default=1, type=int,
                        help='set the desired number of channels')
arg_parser.add_argument('--device', default='CPU', type=str,
                        help='set the device(s) on which inference would be executed')
arg_parser.add_argument('--pre_proc', default='ie', type=str,
                        help='set pre-processing method, one of "ie", "opencv", "g-api", "vaapi"')
arg_parser.add_argument('--waiting_time', default=None, type=float,
                        help='set the timer (sec) by which the pipeline must complete its work')
arg_parser.add_argument('--detection_model', default='mobilenet-ssd', type=str,
                        help='set detection model')
arg_parser.add_argument('--classification_models', default='', type=str,
                        help='set classification model')

argv = arg_parser.parse_args()


def env_var_value(env_var):
    try:
        env_var_values = os.environ[env_var].split(':')
    except KeyError:
        env_var_values = None
    return env_var_values


def get_model_path(model):
    if os.path.isfile(model) and model.endswith(".xml"):
        return model
    models_path_list = env_var_value('MODELS_PATH')

    model_conf = model.split(':')
    model_name = model_conf[0]
    model_precision = "FP32"
    if len(model_conf) > 1:
        model_precision = model_conf[1]

    for models_path in models_path_list:
        for path, subdirs, files in os.walk(models_path):
            for name in files:
                if ((model_precision.upper() in path or model_precision.lower() in path)
                        and model_name in name and name.endswith(".xml")):
                    return (os.path.join(path, name))

    raise ValueError("Model was not found. Check your MODELS_PATH={} environment variable or model's name ({}) & precision ({})".format(
        models_path_list, model_name, model_precision))


def get_model_proc_path(model_name):
    if os.path.isfile(model_name) and model_name.endswith(".json"):
        return model_name
    models_procs_path = env_var_value("MODELS_PROC_PATH")
    if models_procs_path:
        for model_proc_path in models_procs_path:
            for path, subdirs, files in os.walk(model_proc_path):
                for name in files:
                    if (model_name in name and name.endswith(".json")):
                        return (os.path.join(path, name))
    else:
        pwd = env_var_value("PWD")[0]
        gst_va_path = pwd[:pwd.find(
            "gst-video-analytics") + len("gst-video-analytics")]
        samples_path = gst_va_path + "/samples"
        for path, subdirs, files in os.walk(samples_path):
            for name in files:
                if (model_name in name and name.endswith(".json")):
                    return (os.path.join(path, name))
    return None


def get_fps_list_from_str_log(log_string):
    __log_string = log_string
    if type(__log_string) is bytes:
        __log_string = __log_string.decode("utf-8")
    lines_of_log_string = __log_string.split('\n')
    fps_string = []
    per_stream_metrics = []

    for line in lines_of_log_string:
        if "average" not in line:
            fps_string += re.findall("per-stream=.*fps", line)

    for item in fps_string:
        per_stream_metrics.append((float("".join(re.findall("[0-9.]", item)))))

    return per_stream_metrics


def create_pipeline():
    global argv
    pipeline = "--gst-plugin-path {} ".format(
        env_var_value('GST_PLUGIN_PATH')[0])

    if argv.video_path.startswith("/dev/video"):
        pipeline += "v4l2src device={} ! ".format(argv.video_path)
    elif argv.video_path.startswith("rtsp://"):
        pipeline += "urisourcebin uri={} ! ".format(argv.video_path)
    else:
        pipeline += "filesrc location={} ! ".format(argv.video_path)

    pipeline += "decodebin ! "
    if argv.pre_proc == "vaapi":
        pipeline += "vaapipostproc ! video/x-raw(memory:VASurface) ! "
    else:
        pipeline += "videoconvert ! video/x-raw,format=BGRx ! "

    detect_model_path = get_model_path(argv.detection_model)
    detect_model_proc_path = get_model_proc_path(argv.detection_model)

    if detect_model_proc_path:
        pipeline += "gvadetect model={} model-proc={} device={} pre-proc={} ! ".format(
            detect_model_path, detect_model_proc_path, argv.device, argv.pre_proc)
    else:
        pipeline += "gvadetect model={} device={} pre-proc={} ! ".format(
            detect_model_path, argv.device, argv.pre_proc)

    pipeline += "queue ! "

    if argv.classification_models:
        for classification_model in argv.classification_models.split(','):
            object_class = ""
            if '(' in classification_model and ')' in classification_model:
                object_class = classification_model[classification_model.find(
                    '(')+1:classification_model.find(')')]
                classification_model = classification_model[:classification_model.find(
                    '(')]
            classif_model_path = get_model_path(classification_model)
            classif_model_proc_path = get_model_proc_path(classification_model)

            if classif_model_proc_path:
                pipeline += "gvaclassify model={} model-proc={} device={} pre-proc={} ".format(
                    classif_model_path, classif_model_proc_path, argv.device, argv.pre_proc)
            else:
                pipeline += "gvaclassify model={} device={} pre-proc={} ".format(
                    classif_model_path, argv.device, argv.pre_proc)
            if object_class:
                pipeline += "object-class={} ! queue ! ".format(object_class)
            else:
                pipeline += "! queue ! "

    pipeline += "gvafpscounter ! "
    if argv.set_gui:
        if argv.pre_proc == "vaapi":
            pipeline += "vaapipostproc ! video/x-raw ! "
        pipeline += "gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false "
    else:
        pipeline += "fakesink sync=false "

    pipeline *= argv.num_channels

    return pipeline


def is_still_running(pid):
    if os.path.isdir('/proc/{}'.format(pid)):
        return True
    return False


def run(pipeline):
    global argv
    gst_launch_process = subprocess.Popen(("gst-launch-1.0 " + pipeline).split(' '),
                                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                          preexec_fn=os.setsid)
    pid = os.getpgid(gst_launch_process.pid)

    try:
        stdout, stderr = gst_launch_process.communicate(
            timeout=argv.waiting_time)
    except subprocess.TimeoutExpired:
        gst_launch_process.terminate()
        stdout, stderr = gst_launch_process.communicate()
        if is_still_running(pid):
            os.killpg(pid, signal.SIGKILL)
    except KeyboardInterrupt:
        gst_launch_process.kill()
        stdout, stderr = gst_launch_process.communicate()
        if is_still_running(pid):
            os.killpg(pid, signal.SIGKILL)

    print("\n\t", stdout.decode('utf-8'))
    print("return code:\t", gst_launch_process.returncode)
    if gst_launch_process.returncode is not 0:
        print("\n\t", stderr.decode('utf-8'))

    return get_fps_list_from_str_log(stdout.decode('utf-8'))


if __name__ == "__main__":

    pipeline = create_pipeline()
    print("Your pipeline:\n\t", pipeline)
    fps_per_stream_list = run(pipeline)
    if fps_per_stream_list:
        average_fps = sum(fps_per_stream_list)/len(fps_per_stream_list)
        print("Average FPS:\t", average_fps)
