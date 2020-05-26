from os.path import isfile, isdir, join, realpath
from os import listdir, environ
import shlex
import subprocess
import pickle
import json
from pprint import pprint
from sklearn.metrics import classification_report, f1_score, precision_recall_fscore_support
from sklearn.svm import SVC
from sklearn.ensemble import RandomForestClassifier
import pickle as pkl
import time
import numpy as np
from copy import copy
MODEL_PATH = '/root/Projects/models/intel/person-detection-retail-0013/FP32/person-detection-retail-0013.xml'
DATASET_PATH = "/root/Projects/train/"
ALPHA = 0.1
ALPHA_HW = 0.01
RES_PATH='/root/Projects/gst-video-analytics-0.7.0/samples/people_on_stairs/classify_overspeeding/res.json'


SVM_PATH = '/root/Projects/models/overspeed_classify/RF_classifier.sav'

IDENTIFY_PIPELINE_TEMPLATE = """gst-launch-1.0 filesrc \
        location={} \
        ! decodebin  ! videoconvert ! video/x-raw,format=BGRx ! gvadetect  \
        model={} \
        ! gvaspeedometer alpha={} alpha-hw={} interval=0.03333333 \
        ! gvapython module={} class=OverspeedClassifier   \
        ! fakesink sync=false"""


class OverspeedClassifier():
    def __init__(self):
        print("Initializing python OverspeedClassifier module")
        print(MODEL_PATH)
        
        print("Classification model loaded")
        self.velocities = []
        self._result_path = RES_PATH
        self.frames_processed = 0

    def process_frame(self, frame):

        for region in frame.regions():
            for tensor in region.tensors():
                if tensor.has_field("velocity"):
                    print(tensor['velocity'])
                    self.velocities.append(tensor['velocity'])

 

        self.__updateJSON() 
        self.frames_processed += 1

    def __updateJSON(self):
        with open(self._result_path, "w") as write_file:
            json.dump(self.velocities,
                      write_file, indent=4, sort_keys=True)

    def __dump_data(self):
        with open(self._result_path, "w") as write_file:
            write_file.write("{} \t".format(self.velocities))


if __name__ == "__main__":
    svclassifier = pickle.load(open(SVM_PATH, 'rb'))
    result_path = 'res.txt'
    for file_name in listdir(DATASET_PATH):
        if file_name.endswith(".mp4"):
            video_path = join(DATASET_PATH, file_name)
            pipeline_str = IDENTIFY_PIPELINE_TEMPLATE.format(
                video_path,
                MODEL_PATH,
                ALPHA,
                ALPHA_HW,
                realpath(__file__),
                
            )
            print(pipeline_str)
            proc = subprocess.run(
                shlex.split(pipeline_str), env=environ.copy())

            with open("classify_overspeed.txt", 'a') as f:
                f.write("{} evaluated\n".format(file_name))
            if proc.returncode != 0:
                print("Error while running pipeline")
                exit(-1)
            with open(RES_PATH, "r") as pr_res_file:
                _raw_result = json.load(pr_res_file)
                velocity = np.array(_raw_result)

                hist, bin_edges = np.histogram(velocity, bins=20)
                norm = np.linalg.norm(hist) + 1e-6
                hist = hist.astype(np.float32)

                hist /= norm
                hist = hist.reshape(1, len(hist))

                y_pred = svclassifier.predict_proba(hist)

                y_pred = True if y_pred[0, 1] >= 0.3 else False
                pred_file = video_path.replace(".mp4", 'predict.txt')
                if y_pred:
                    with open(pred_file, 'a') as f:
                        f.write("Current ID violates speed limit\n")
                else:
                    with open(pred_file, 'a') as f:
                        f.write("Current ID does not violate speed limit\n")
            
                # velocity = np.array(velocities)

                # hist, bin_edges = np.histogram(velocity, bins=20)
                # norm = np.linalg.norm(hist) + 1e-6
                # hist = hist.astype(np.float32)

                # for i, elem in enumerate(hist):
                #     hist[i] = float(elem) / norm
                # hist = hist.reshape(1, len(hist))

                # y_pred = self.svclassifier.predict_proba(hist)

                # y_pred = True if y_pred[0, 1] >= 0.3 else False
                # if y_pred:
                #     with open("classify_overspeed.txt", 'a') as f:
                #         f.write("Current ID violates speed limit\n")
                # else:
                #     with open("classify_overspeed.txt", 'a') as f:
                #         f.write("Current ID does not violate speed limit\n")
                # self.velocities = []

            # print(str(proc.stdout))
            # if 'does not violate' in str(proc.stdout) and 'fast' in file_name:
            #     FN += 1
            # if 'violates' in str(proc.stdout) and 'fast' in file_name:
            #     TP += 1
            # if 'violates' in str(proc.stdout) and 'regular' in file_name:
            #     FP += 1