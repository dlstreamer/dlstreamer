# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import inspect

from enum import Enum
from dataclasses import dataclass, field


@dataclass
class BBox:
    xmin: float = -1
    ymin: float = -1
    xmax: float = -1
    ymax: float = -1
    prob: float = -1
    label: str = None
    classes: list = field(default_factory=list)
    additional_meta: list = field(default_factory=list)

    def is_invalid(self) -> bool:
        return self.prob < 0

    def init_label(self):
        for key, value in self.classes:
            if key == 'label':
                self.label = value
                return


class BaseProviderMeta(type):
    def __new__(mcs, name, bases, attrs, **kwargs):
        cls = super().__new__(mcs, name, bases, attrs)
        # do not create container for abstract provider
        if '_is_base_provider' in attrs:
            return cls
        assert issubclass(cls, BaseProvider), "Do not use metaclass directly"
        cls.register(cls)
        return cls


class BaseProvider(metaclass=BaseProviderMeta):
    _is_base_provider = True
    registry = {}
    __action_name__ = None

    @classmethod
    def register(cls, provider):
        provider_name = getattr(cls, '__action_name__')
        if not provider_name:
            return
        cls.registry[provider_name] = provider

    @classmethod
    def provide(cls, provider, *args, **kwargs):
        if provider not in cls.registry:
            raise ValueError(
                "Requested provider {} not registered".format(provider))
        root_provider = cls.registry[provider]
        root_provider.validate()
        return root_provider(*args, **kwargs)


class ClassProvider(BaseProvider):
    __step_name__ = "compare"
    registry = {}

    @classmethod
    def validate(cls):
        methods = [
            f[0] for f in inspect.getmembers(cls, predicate=inspect.isfunction)
        ]
        if 'compare' not in methods:
            raise AttributeError(
                "Requested class {} registred as '{}' doesn't provide required method compare"
                .format(cls.__name__, cls.__action_name__))


class CheckLevel(Enum):
    soft = "soft"
    full = "full"

    def __str__(self):
        return self.value


class CheckInfoStorage:
    def __init__(self, error_thr=0.1):
        self._error_thr = error_thr
        self.checks = 0
        self.fails = 0

    def is_passed(self):
        if self.checks == 0:
            return True
        return self.fails / self.checks < self._error_thr


def fails_percent(*storages: CheckInfoStorage):
    if sum([storage.checks for storage in storages]) == 0:
        return 0
    return round(sum([storage.fails for storage in storages]) / sum([storage.checks for storage in storages]) * 100)
