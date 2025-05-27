# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import logging

from video_comparators.utils import ClassProvider, CheckLevel, CheckInfoStorage
from utils import GTComparisionError


class ObjectClassificationComparator(ClassProvider):
    __action_name__ = "object_classification"

    def __init__(self, mode: CheckLevel, logger: logging.Logger = None):
        self._logger = logger if logger else logging.getLogger()
        self._mode = mode

    def compare(self, reference: list, prediction: list, mode: CheckLevel, meta_checks_storage: CheckInfoStorage):
        # for soft comparing we store number of checks (size of referenced list of labels),
        # also consider difference between sizes of referenced and predicted lists in number of fails
        if mode == CheckLevel.soft:
            meta_checks_storage.checks += len(reference)
            meta_checks_storage.fails += abs(len(reference) - len(prediction))

        # try to find referenced class names in predicted list and compare value
        for ref_cl in reference:
            # First, try to match by name
            pred_cl = next((cl for cl in prediction if cl[0] == ref_cl[0]), None)
            if not pred_cl and len(ref_cl) > 1:
                # If not found try value instead.
                # This is to find matches if names of objects are different
                # This is possible due Arch 1.0 to Arch 2.0 runs comparsion
                pred_cl = next((cl for cl in prediction if len(cl) > 1 and cl[1] == ref_cl[1]), None)

            error_msg = None
            if pred_cl:
                # Found
                prediction.remove(pred_cl)
                # Test if names are similar
                similar = ref_cl[0] in pred_cl[0] or pred_cl[0] in ref_cl[0]
                if not similar:
                    error_msg = "Found match but object names are different:\n  {} not like {}".format(
                        ref_cl[0], pred_cl[0])
            else:
                error_msg = "Classes set differs. Couldn't find match for:\n  {}".format(ref_cl)
                error_msg += "\nitems left in prediction:\n  {}".format("\n  ".join(map(str, prediction)))

            if error_msg:
                if mode == CheckLevel.soft:
                    meta_checks_storage.fails += 1
                    self._logger.warn(error_msg)
                if mode == CheckLevel.full:
                    raise GTComparisionError(error_msg)

