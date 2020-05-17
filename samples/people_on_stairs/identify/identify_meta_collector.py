
import json
import gstgva as va

DATASET_PATH = "/path/to/scientific_work/dataset/our_dataset/"
D_MODEL_PATH = "/path/to/gva/data/models/intel/face-detection-adas-0001/FP32/face-detection-adas-0001.xml"
L_MODEL_PATH = "/path/to/gva/data/models/intel/landmarks-regression-retail-0009/FP32/landmarks-regression-retail-0009.xml"
L_MODEL_PROC_PATH = "../../gst_launch/reidentification/model_proc/landmarks-regression-retail-0009.json"
I_MODEL_PATH = "/path/to/gva/data/models/intel/face-reidentification-retail-0095/FP32/face-reidentification-retail-0095.xml"
I_MODEL_PROC_PATH = "../../gst_launch/reidentification/model_proc/face-reidentification-retail-0095.json"
GALLERY_PATH = "./gallery/gallery.json"

RESULT_JSON_DIR_PATH = "./identify_meta/"
RESULT_JSON_PATH = "./identify_meta.json"

IDENTIFY_PIPELINE_TEMPLATE = """gst-launch-1.0 \
filesrc location={} ! decodebin ! videoconvert ! video/x-raw,format=BGRx ! \
gvadetect model={} device=CPU pre-process-backend=opencv ! queue ! \
gvaclassify model={} model-proc={} device=CPU pre-process-backend=opencv ! queue ! \
gvaclassify model={} model-proc={} device=CPU pre-process-backend=opencv ! queue ! \
gvaidentify gallery={} cosine-distance-threshold=0.0 ! \
gvapython module={} class=IdentifyMetaCollector arg=[\\"{}\\"] ! \
fakesink sync=false"""


class IdentifyMetaCollector:
    def __init__(self, argument):
        self._data = dict()
        self._frame_number = 0
        self._result_json_path = argument

    def process_frame(self, frame):
        for region in frame.regions():
            for tensor in region.tensors():
                if tensor.has_field('attribute_name') and tensor.has_field('format') \
                        and tensor.has_field('label') and tensor.has_field('label_id') \
                        and tensor.has_field('cos_dist'):
                    if tensor['attribute_name'] == 'face_id' and tensor['format'] == 'cosine_distance':
                        self._data[self._frame_number] = {
                            "label": tensor['label'],
                            "id": tensor['label_id'],
                            "cosine_distance": tensor['cos_dist']
                        }

        self._frame_number += 1
        self.__updateJSON()

    def __updateJSON(self):
        with open(self._result_json_path, "w") as write_file:
            json.dump(self._data,
                      write_file, indent=4, sort_keys=True)

    def data(self):
        return self._data


import subprocess
import shlex
from os import listdir, environ
from os.path import isfile, isdir, join, realpath

if __name__ == "__main__":
    for file_name in listdir(DATASET_PATH):
        video_path = join(DATASET_PATH, file_name)
        pipeline_str = IDENTIFY_PIPELINE_TEMPLATE.format(
            video_path, D_MODEL_PATH, L_MODEL_PATH, L_MODEL_PROC_PATH,
            I_MODEL_PATH, I_MODEL_PROC_PATH, GALLERY_PATH,
            realpath(__file__), RESULT_JSON_DIR_PATH + file_name[:-4] + '.json')
        print(pipeline_str)
        proc = subprocess.Popen(
            shlex.split(pipeline_str), shell=False, env=environ.copy())
        if proc.wait() != 0:
            print("Error while running pipeline")
            exit(-1)

    result = {}
    for file_name in listdir(RESULT_JSON_DIR_PATH):
        with open(join(RESULT_JSON_DIR_PATH, file_name), "r") as read_file:
            result[file_name[:-5]] = json.load(read_file)

    with open(RESULT_JSON_PATH, "w") as write_file:
        json.dump(result, write_file, indent=4, sort_keys=True)
