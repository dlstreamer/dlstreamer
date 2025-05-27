# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import itertools
import functools
import operator


class CaseGenerator:
    @staticmethod
    def get_keys_and_values(container):
        if isinstance(container, dict):
            return container.keys(), [val if isinstance(val, list) else [val] for val in container.values()]

        # List logic
        def _remove_duplicates(values):
            values.sort()
            return [val for val, _ in itertools.groupby(values)]

        keys = set()
        result_keys = []
        result_values = []
        for (key, val) in container[::-1]:
            if not isinstance(key, list):
                key = [key]
                if not isinstance(val, list):
                    val = [val]
                val = [[v] for v in val]
            selection = []
            for i, k in enumerate(key):
                if k not in keys:
                    selection.append(i)
                keys.add(k)
            if selection:
                result_keys.append([key[i] for i in selection])
                result_values.append(_remove_duplicates([[v[i] for i in selection] for v in val]))
        return result_keys[::-1], result_values[::-1]

    @staticmethod
    def flatten(container):
        res = []
        for (key, val) in container:
            if isinstance(key, tuple) or isinstance(key, list):
                for elem in zip(key, val):
                    res.append(elem)
            else:
                res.append((key, val))
        return res

    @staticmethod
    def estimate_count(testset_descriptor: list):
        _keys, values = CaseGenerator.get_keys_and_values(testset_descriptor)
        return functools.reduce(
            operator.mul,
            [len(elem) for elem in values],
            1
        )

    @staticmethod
    def generate(testset_descriptor: list):
        keys, values = CaseGenerator.get_keys_and_values(testset_descriptor)
        for element in itertools.product(*values):
            yield dict(CaseGenerator.flatten(
                zip(keys, element)))
