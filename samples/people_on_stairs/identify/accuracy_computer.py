import json
import math
import numpy as np

from utils import PR_GT_MATCH_TABLE, GT_PR_MATCH_TABLE
PR_RESULT_FILE = "./identify_meta.json"
PERSONS_NUMBER = 7


class IdentifyMetaResult:
    def __init__(self, raw_data=None):
        self._raw_result = raw_data
        self._result = dict()
        pass

    @classmethod
    def normalize(cls, vector):
        norm = np.linalg.norm(vector)
        if norm == 0:
            return vector
        return vector / norm

    # @classmethod
    # def hardVotingMethod(cls, raw_res, normalize=True):
    #     cos_dist_list = np.array([res['cosine_distance'] for res in raw_res])
    #     if normalize:
    #         cos_dist_list = cls.normalize(cos_dist_list)

    #     vote_list = np.array([0 for _ in range(PERSONS_NUMBER)])
    #     for i, res in enumerate(raw_res):
    #         vote_list[res['id'] - 1] += cos_dist_list[i]

    #     person_id = np.argmax(vote_list) + 1
    #     return person_id

    # @classmethod
    # def softVotingMethod(cls, raw_res, normalize=True):
    #     cos_dist_exp_list = np.array([math.exp(res['cosine_distance'])
    #                                   for res in raw_res])
    #     cos_dist_soft_max_list = cos_dist_exp_list / np.sum(cos_dist_exp_list)
    #     if normalize:
    #         cos_dist_soft_max_list = cls.normalize(cos_dist_soft_max_list)

    #     vote_list = np.array([0 for _ in range(PERSONS_NUMBER)])
    #     for i, res in enumerate(raw_res):
    #         vote_list[res['id'] - 1] += cos_dist_soft_max_list[i]

    #     person_id = np.argmax(vote_list) + 1
    #     return person_id

    # @classmethod
    # def softMaxMethod(cls, raw_res, normalize=True):
    #     cos_dist_exp_list = np.array([math.exp(res['cosine_distance'])
    #                                   for res in raw_res])
    #     cos_dist_soft_max_list = cos_dist_exp_list / np.sum(cos_dist_exp_list)
    #     if normalize:
    #         cos_dist_soft_max_list = cls.normalize(cos_dist_soft_max_list)

    #     person_id = raw_res[np.argmax(cos_dist_soft_max_list)]['id']
    #     return person_id

    @classmethod
    def hardMaxMethod(cls, raw_res, normalize=True):
        cos_dist_list = np.array([res['cosine_distance'] for res in raw_res])
        if normalize:
            cos_dist_list = cls.normalize(cos_dist_list)

        person_id = raw_res[np.argmax(cos_dist_list)]['id']
        return person_id

    def postProcessing(self, method, normalize=False, pr_gt_match_table=None):
        if not self._raw_result:
            raise ValueError
        for video_name, raw_res in self._raw_result.items():
            raw_res = [{'cosine_distance': res['cosine_distance'],
                        'id': res['id']} for res in raw_res.values()]
            if pr_gt_match_table:
                self._result[video_name] = pr_gt_match_table[method(
                    raw_res, normalize)]
            else:
                self._result[video_name] = method(raw_res, normalize)

    # return: dict {video_name: person_id}
    def getResult(self):
        return self._result

    def readRawResultJSON(self, path):
        with open(path, "r") as pr_res_file:
            self._raw_result = json.load(pr_res_file)


class AccuracyComputer:
    def __init__(self, pr_res, gt_res=None):
        self._pr_res = pr_res

    def calculate(self):
        right_answers_number = 0
        per_class_right_answers_number = {}
        per_class_common_answers_number = {}

        for video_name, pr_person_id in self._pr_res.items():
            gt_person_id = int(video_name[:3])
            if gt_person_id in per_class_common_answers_number:
                per_class_common_answers_number[gt_person_id] += 1
            else:
                per_class_common_answers_number[gt_person_id] = 1

            if gt_person_id == pr_person_id:
                right_answers_number += 1
                if gt_person_id in per_class_right_answers_number:
                    per_class_right_answers_number[gt_person_id] += 1
                else:
                    per_class_right_answers_number[gt_person_id] = 1

        per_class_accuracy = {}
        for person_id, person_id_right_answers_number in per_class_right_answers_number.items():
            per_class_accuracy[person_id] = person_id_right_answers_number / \
                per_class_common_answers_number[person_id]

        common_accuracy = right_answers_number / len(self._pr_res)
        return common_accuracy, per_class_accuracy


if __name__ == "__main__":
    pr_res = IdentifyMetaResult()
    pr_res.readRawResultJSON(PR_RESULT_FILE)
    pr_res.postProcessing(IdentifyMetaResult.hardMaxMethod,
                          normalize=True, pr_gt_match_table=PR_GT_MATCH_TABLE)
    accuracy_computer = AccuracyComputer(pr_res.getResult())
    common_accuracy, per_class_accuracy = accuracy_computer.calculate()
    print(common_accuracy)

    per_person_accuracy = {}
    for class_id, accuracy in per_class_accuracy.items():
        per_person_accuracy[GT_PR_MATCH_TABLE[class_id]['label']] = accuracy

    all_persons = [id_label['label']
                   for id_label in GT_PR_MATCH_TABLE.values()]
    for person in all_persons:
        if not person in per_person_accuracy.keys():
            per_person_accuracy[person] = 0.0

    print(per_person_accuracy)
