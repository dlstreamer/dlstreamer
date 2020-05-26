from os.path import isfile, isdir, join, realpath
from os import listdir, environ
import shlex
import subprocess
import pickle
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


SVM_PATH = '/root/Projects/RF_classifier.sav'

IDENTIFY_PIPELINE_TEMPLATE = """gst-launch-1.0 filesrc \
        location={} \
        ! decodebin  ! videoconvert ! video/x-raw,format=BGRx ! gvadetect  \
        model={} \
        ! gvaspeedometer alpha={} alpha-hw={}  \
        ! gvapython module={} class=OverspeedClassifier \
        ! fakesink sync=false"""


class OverspeedClassifier():
    def __init__(self):
        print("Initializing python OverspeedClassifier module")
        self.svclassifier = pickle.load(open(SVM_PATH, 'rb'))
        print("Claasification model loaded")
        self.velocities = []
        self.frames_processed = 1

    def process_frame(self, frame):

        for region in frame.regions():
            for tensor in region.tensors():
                if not tensor.has_field("velocity"):
                    continue

                if self.frames_processed % 140:
                    self.velocities.append(copy(tensor['velocity']))
                    continue
                velocity = np.array(self.velocities)

                hist, bin_edges = np.histogram(velocity, bins=20)
                norm = np.linalg.norm(hist) + 1e-6
                hist = hist.astype(np.float32)

                for i, elem in enumerate(hist):
                    hist[i] = float(elem) / norm
                hist = hist.reshape(1, len(hist))

                y_pred = self.svclassifier.predict_proba(hist)

                y_pred = True if y_pred[0, 1] >= 0.3 else False
                if y_pred:
                    with open("classify_overspeed.txt", 'a') as f:
                        f.write("Current ID violates speed limit\n")
                else:
                    with open("classify_overspeed.txt", 'a') as f:
                        f.write("Current ID does not violate speed limit\n")
                self.velocities = []

        self.frames_processed += 1


if __name__ == "__main__":
    FP, FN, TP = 0, 0, 0
    for file_name in listdir(DATASET_PATH):
        if file_name.endswith(".mp4"):
            video_path = join(DATASET_PATH, file_name)
            pipeline_str = IDENTIFY_PIPELINE_TEMPLATE.format(
                video_path,
                MODEL_PATH,
                ALPHA,
                ALPHA_HW,
                realpath(__file__)
            )
            # print(pipeline_str)
            proc = subprocess.run(
                shlex.split(pipeline_str), env=environ.copy())

            with open("classify_overspeed.txt", 'a') as f:
                f.write("{} evaluated\n".format(file_name))
            if proc.returncode != 0:
                print("Error while running pipeline")
                exit(-1)
            # print(str(proc.stdout))
            # if 'does not violate' in str(proc.stdout) and 'fast' in file_name:
            #     FN += 1
            # if 'violates' in str(proc.stdout) and 'fast' in file_name:
            #     TP += 1
            # if 'violates' in str(proc.stdout) and 'regular' in file_name:
            #     FP += 1
    F1 = 2 * TP / (2 * TP + FP + FN)
    print(F1)

