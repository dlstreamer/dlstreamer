import subprocess
import json
import numpy as np
import pickle
from collections import Counter

import os
from os import environ

DATASET_PATH = "/root/study/our_dataset/"

SPEED_OUTPUT_PATH = "./output/speed_meta.json"
FACE_OUTPUT_PATH = "./output/face_meta.json"
HAND_OUTPUT_PATH = "./output/hand_meta.json"
ANALYTICS_OUTPUT_PATH = "./output/result.json"

PIPELINE_TEMPLATE = """gst-launch-1.0 filesrc location={} ! decodebin ! videoconvert ! video/x-raw,format=BGR ! tee name=tp \
tp. ! $HAND_DETECTION ! $FINALIZE \
tp. ! $SPEED_DETECTION ! $FINALIZE \
tp. ! $FACE_DETECTION ! $FINALIZE ! fakesink sync=false"""

OVERSPEED_SVM_PATH = environ["MODELS_PATH"] + "/overspeed/overspeed_classify_RF_classifier.sav"
SVCLASSFIER = None


def analyze_speed():
    global SVCLASSFIER
    if not SVCLASSFIER:
        SVCLASSFIER = pickle.load(open(OVERSPEED_SVM_PATH, 'rb'))

    with open(SPEED_OUTPUT_PATH, 'r') as f:
        raw_result = json.load(f)
        velocity = np.array(raw_result)

        hist, bin_edges = np.histogram(velocity, bins=20)
        norm = np.linalg.norm(hist) + 1e-6
        hist = hist.astype(np.float32)

        hist /= norm
        hist = hist.reshape(1, len(hist))

        y_pred = SVCLASSFIER.predict_proba(hist)
        result = True if y_pred[0, 1] >= 0.3 else False
        return result


def analyze_face():
    with open(FACE_OUTPUT_PATH, 'r') as f:
        raw_result = json.load(f)

        cos_dist_list = np.array([rec['cosine_distance'] for rec in raw_result])
        person_id = raw_result[np.argmax(cos_dist_list)]['id']
        person = raw_result[np.argmax(cos_dist_list)]['label']
        return person_id, person


def analyze_hands():
    with open(HAND_OUTPUT_PATH, 'r') as f:
        raw_result = json.load(f)
        left_hand_data = raw_result['left_hand']
        right_hand_data = raw_result['right_hand']
        left_count = Counter(left_hand_data)
        right_count = Counter(right_hand_data)

        left_hand_pred = left_count.most_common(1)[0][0]
        right_hand_pred = right_count.most_common(1)[0][0]

        railing = left_hand_pred == 'railing' or right_hand_pred == 'railing'

        return left_hand_pred, right_hand_pred, railing


ID_TO_PERSON_MAP = {
    1: {
        "id": 4,
        "label": "Victor"
    },
    2: {
        "id": 6,
        "label": "Nikolai"
    },
    3: {
        "id": 3,
        "label": "Oleg"
    },
    4: {
        "id": 1,
        "label": "Boris"
    },
    5: {
        "id": 7,
        "label": "Dmitry"
    },
    6: {
        "id": 5,
        "label": "Vladimir"
    },
    7: {
        "id": 2,
        "label": "Pavel"
    },
}


def get_actual_features_by_filename(filename):
    # 004_regular_free_holdnot_up.mp4
    features = filename[:-4].split('_')
    model_id = int(features[0])
    speed = features[1]

    feature2 = features[2] if features[2] != 'free' else 'empty'

    if features[4] == 'up':
        left_hand = 'railing' if features[3] == 'hold' else 'empty'
        right_hand = feature2

    else:
        left_hand = feature2
        right_hand = 'railing' if features[3] == 'hold' else 'empty'

    return {
        'id': ID_TO_PERSON_MAP[model_id]['id'],
        'label': ID_TO_PERSON_MAP[model_id]['label'],
        'speed': speed,
        'left_hand': left_hand,
        'right_hand': right_hand,
        'railing': True if features[3] == 'hold' else False
    }


if __name__ == '__main__':
    dataset = os.listdir(environ.get('DATASET_PATH', DATASET_PATH))
    dataset_count = len(dataset)

    print(f"Found {dataset_count} samples in dataset.")

    results = {
        'accuracy': {
            'overall': 0,
            'id': 0,
            'speed': 0,
            'left_hand': 0,
            'right_hand': 0,
            'railing': 0
        },
        'matches': {
            'overall': 0,
            'id': 0,
            'speed': 0,
            'left_hand': 0,
            'right_hand': 0,
            'railing': 0
        },
        'samples': []
    }

    for i in range(0, dataset_count):
        print(f"Running sample {i+1}/{dataset_count}")

        file_path = DATASET_PATH + dataset[i]

        print(f"Sample: {file_path}")

        actual_features = get_actual_features_by_filename(dataset[i])

        pipeline = PIPELINE_TEMPLATE.format(file_path)
        proc = subprocess.Popen(pipeline, shell=True, env=environ.copy())

        if proc.wait() != 0:
            print("Error while running pipeline")
            exit(-1)

        person = analyze_face()
        speed = 'fast' if analyze_speed() else 'regular'
        left_hand, right_hand, railing = analyze_hands()

        pred = {
            'id': person[0],
            'label': person[1],
            'speed': speed,
            'left_hand': left_hand,
            'right_hand': right_hand,
            'railing': railing
        }

        local_res = {"actual": actual_features, "pred": pred}

        print(local_res)

        results['samples'].append(local_res)

        if actual_features == pred:
            results['matches']['overall'] += 1
        if actual_features['id'] == pred['id']:
            results['matches']['id'] += 1
        if actual_features['speed'] == pred['speed']:
            results['matches']['speed'] += 1
        if actual_features['left_hand'] == pred['left_hand']:
            results['matches']['left_hand'] += 1
        if actual_features['right_hand'] == pred['right_hand']:
            results['matches']['right_hand'] += 1
        if actual_features['railing'] == pred['railing']:
            results['matches']['railing'] += 1

    print("Done!")

    results['accuracy']['overall'] = results['matches']['overall'] / dataset_count
    results['accuracy']['id'] = results['matches']['id'] / dataset_count
    results['accuracy']['speed'] = results['matches']['speed'] / dataset_count
    results['accuracy']['left_hand'] = results['matches']['left_hand'] / dataset_count
    results['accuracy']['right_hand'] = results['matches']['right_hand'] / dataset_count
    results['accuracy']['railing'] = results['matches']['railing'] / dataset_count

    print("Accuracy: ")
    print(results['accuracy'])

    with open(ANALYTICS_OUTPUT_PATH, 'w+') as f:
        json.dump(results, f)





