import json
import random

RESULTS_FILE = "output/result.json"


def prepare_dataset(balanced=False, seed=42):
    with open(RESULTS_FILE, 'r') as f:
        raw_result = json.load(f)
        if not balanced:
            return raw_result['samples'], raw_result['samples'].keys()

        raw_samples = raw_result['samples']
        raw_keys = list(raw_samples.keys())

        random.seed(seed)
        random.shuffle(raw_keys)

        no_violation_ratio = 0.5
        speed_violation_ratio = 0.15
        railing_violation_ratio = 0.15
        other_stuff_violation_ratio = 0.15
        mixed_violation_ratio = 0.05

        no_violation_list = [k for k in raw_keys if 'regular_free_hold_' in k]
        no_violation_list_count = len(no_violation_list)

        total_samples_count = no_violation_list_count * 2

        speed_violation_list = [k for k in raw_keys if 'fast_free_hold_' in k]
        speed_violation_list_count = round(total_samples_count * speed_violation_ratio)
        speed_violation_list = speed_violation_list[:speed_violation_list_count]

        railing_violation_list = [k for k in raw_keys if 'regular_free_holdnot' in k]
        railing_violation_list_count = round(total_samples_count * railing_violation_ratio)
        railing_violation_list = railing_violation_list[:railing_violation_list_count]

        other_stuff = ["cup", "others", "phone"]
        other_stuff_violation_list = [k for k in raw_keys if 'regular' in k and 'hold_' in k and k.split('_')[2] in other_stuff]
        other_stuff_violation_list_count = round(total_samples_count * other_stuff_violation_ratio)
        other_stuff_violation_list = other_stuff_violation_list[:other_stuff_violation_list_count]

        mixed_violation_list = [k for k in raw_keys
                                if k not in no_violation_list
                                and k not in speed_violation_list
                                and k not in railing_violation_list
                                and k not in other_stuff_violation_list]

        mixed_violation_list_count = round(total_samples_count*mixed_violation_ratio)
        mixed_violation_list = mixed_violation_list[:mixed_violation_list_count]

        balanced_keys = []
        balanced_keys.extend(no_violation_list)
        balanced_keys.extend(speed_violation_list)
        balanced_keys.extend(railing_violation_list)
        balanced_keys.extend(other_stuff_violation_list)
        balanced_keys.extend(mixed_violation_list)

        return raw_samples, balanced_keys


def analyze_sample_entries(keys):
    print("Features distribution:")

    speed_violation = 0
    other_stuff_violation = 0
    railing_violation = 0
    overall_violation = 0

    key_count = len(keys)

    for key in keys:
        if 'fast' in key:
            speed_violation += 1
        if 'free' not in key:
            other_stuff_violation += 1
        if 'holdnot' in key:
            railing_violation += 1
        if 'regular_free_hold_' not in key:
            overall_violation += 1

    print(f"Overall violation:\nYES: {overall_violation}\tNO: {key_count - overall_violation}")
    print(f"Speed violation:\nYES: {speed_violation}\tNO: {key_count - speed_violation}")
    print(f"Other stuff violation:\nYES: {other_stuff_violation}\tNO: {key_count - other_stuff_violation}")
    print(f"Railing violation:\nYES: {railing_violation}\tNO: {key_count - railing_violation}")
    print("="*30)


def analyze_metric(data, keys, feature, positive_value):
    print('Analyzing metric: ' + feature + '\n')
    true_positive = 0
    false_positive = 0
    true_negative = 0
    false_negative = 0

    for key in keys:
        sample = data[key]

        actual = sample['actual']
        pred = sample['pred']
        actual_value = actual[feature]
        pred_value = pred[feature]

        if pred_value == positive_value:
            if actual_value == positive_value:
                true_positive += 1
            else:
                false_positive += 1
        else:
            if actual_value == positive_value:
                false_negative += 1
            else:
                true_negative += 1

    accuracy = (true_positive + true_negative) / len(keys)
    precision = true_positive / (true_positive + false_positive)
    recall = true_positive / (false_negative + true_positive)
    f1 = 2 * precision * recall / (precision + recall)

    print(f"Accuracy: {accuracy}")
    print(f"Precision: {precision}")
    print(f"Recall: {recall}")
    print(f"F1-score: {f1}")
    print(f"TP {true_positive}\tFP {false_positive}")
    print(f"FN {false_negative}\tTN {true_negative}")

    print("="*30)


if __name__ == '__main__':
    samples, sample_keys = prepare_dataset(balanced=True, seed=322)
    analyze_sample_entries(sample_keys)

    analyze_metric(samples, sample_keys, 'violation', True)
    analyze_metric(samples, sample_keys, 'railing', True)
    analyze_metric(samples, sample_keys, 'speed', 'fast')
    analyze_metric(samples, sample_keys, 'other_stuff', True)





