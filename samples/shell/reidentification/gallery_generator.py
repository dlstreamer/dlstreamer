#!/bin/python3
# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

import json
import argparse
import os
import subprocess
import re
import fnmatch
 

description = """
Sample tool to generate feature database by image folder using gstreamer pipeline.

The name of the label is chosen as follows:
1) filename - if image is in the root of image folder
2) folder name - if image is in the subfolder
"""

def find_files(directory, pattern='*.*'):
    if not os.path.exists(directory):
        raise ValueError("Directory not found {}".format(directory))

    matches = []
    for root, _, filenames in os.walk(directory):
        for filename in filenames:
            full_path = os.path.join(root, filename)
            if fnmatch.filter([full_path], pattern):
                matches.append(os.path.join(root, filename))
    return matches

def get_models_path():
    models_path = os.getenv("MODELS_PATH", None)

    if models_path is None:
        models_path = os.getenv("INTEL_CVSDK_DIR", None)
        if models_path is not None:
            models_path = os.path.join(models_path, "deployment_tools", "intel_models")
            pass
        pass
    if models_path is None:
        print("Warning: default models path not found by envs MODELS_PATH and INTEL_CVSDK_DIR")
        pass
    return models_path

def find_model_path(model_name, models_dir_list):
    model_path_list = []
    file_pattern = "*{}.xml".format(model_name)
    for models_dir in models_dir_list:
        if not os.path.exists(models_dir):
            continue
        model_path_list += find_files(models_dir, file_pattern)
    return model_path_list

def find_models_paths(model_names, models_dir_list):
    if not model_names:
        raise ValueError("Model names are not set")
    if not models_dir_list:
        raise ValueError("Model directories are not set")        

    d = {}
    for model_name in model_names:
        d[model_name] = None
        model_path_list = find_model_path(model_name, models_dir_list)
        if not model_path_list:
            continue
        if len(model_path_list) > 1:
            print("Warning: Find few models with name: {}. Take the first.".format(model_name))
        d[model_name] = model_path_list.pop(0)
    return d

pipeline_template = "gst-launch-1.0 filesrc location={input_file} ! decodebin ! video/x-raw ! videoconvert ! \
        gvadetect model={detection_model} ! \
        gvaclassify model={identification_model} ! \
        gvametaconvert model={identification_model} converter=tensors-to-file tags={label} location={output_dir} ! \
        fakesink sync=false"
feature_file_regexp_template = r"^{label}_\d+_frame_\d+_idx_\d+.tensor$"


default_detection_model = "face-detection-adas-0001"
default_identification_model = "face-reidentification-retail-0095"
default_models_paths = None if not get_models_path() else get_models_path().split(":")
models_paths = find_models_paths([default_detection_model, default_identification_model], default_models_paths)

default_detection_path = models_paths.get(default_detection_model)
default_identification_path = models_paths.get(default_identification_model)
default_output = os.path.curdir


def parse_arg():
    parser = argparse.ArgumentParser(description=description, formatter_class=argparse.RawTextHelpFormatter)

    parser.add_argument("--source_dir", "-s", required=True, help="Path to the folder with images")
    parser.add_argument("--output", "-o", default=default_output, help="Path to output folder")
    parser.add_argument("--detection", "-d", default=default_detection_path, help="Path to detection model xml file")
    parser.add_argument("--identification", "-i", default=default_identification_path, help="Path to identification model xml file")

    return parser.parse_args()


if __name__ == "__main__":
    args = parse_arg()

    output_path = args.output
    features_out = os.path.join(output_path, "features")
    relative_features_out = os.path.relpath(features_out, output_path)
    os.makedirs(features_out)

    gallery = {}

    for folder, subdir_list, file_list in os.walk(args.source_dir):
        for idx, filename in enumerate(file_list):
            label = os.path.splitext(filename)[0] if folder == args.source_dir else os.path.basename(folder)
            abs_path = os.path.join(os.path.abspath(folder), filename)
            pipeline = pipeline_template.format(input_file=abs_path, detection_model=args.detection,
                                                identification_model=args.identification, label=(label + "_" + str(idx)), output_dir=features_out)
            proc = subprocess.Popen(pipeline, shell=True)
            if proc.wait() != 0:
                print("Error while running pipeline")
                exit(-1)

            if label not in gallery:
                gallery[label] = {'features': []}
                pass
            pass
        pass

    output_files = os.listdir(features_out)

    for label in gallery.keys():
        regexp = re.compile(feature_file_regexp_template.format(label=label))
        gallery[label]['features'] = [os.path.join(relative_features_out, x) for x in output_files if regexp.match(x)]
        pass
    with open(os.path.join(output_path, "gallery.json"), 'w') as f:
        json.dump(gallery, f)
        pass
    pass
