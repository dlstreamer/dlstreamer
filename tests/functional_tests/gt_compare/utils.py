# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
import datetime
import logging
import platform
import colorlog

from tqdm import tqdm

EPOCH = datetime.datetime.utcfromtimestamp(0)


def get_unix_time_millsec():
    dt_now = datetime.datetime.now()
    unix_time = (dt_now - EPOCH).total_seconds() * 1000.0
    return int(unix_time)


class TqdmLoggingHandler(logging.Handler):
    def __init__(self, level=logging.NOTSET):
        super(self.__class__, self).__init__(level)

    def emit(self, record):
        try:
            msg = self.format(record)
            tqdm.write(msg)
            self.flush()
        except (KeyboardInterrupt, SystemExit):
            raise
        except:
            self.handleError(record)

def get_platform_info() -> list:
    return [platform.platform(aliased=True), platform.processor()]


def create_logger(app_name, level='INFO', formatter='%(asctime)s %(levelname)s %(module)s:%(lineno)d - %(message)s'):
    # TODO Make singleton
    color_formatter = colorlog.ColoredFormatter(
        "%(log_color)s%(asctime)s %(levelname)s %(module)s:%(lineno)d - %(message)s",
        datefmt=None,
        reset=True,
        log_colors={
            'DEBUG': 'cyan',
            'INFO': 'green',
            'WARNING': 'yellow',
            'ERROR': 'red',
            'CRITICAL': 'bold_red',
        },
        secondary_log_colors={},
        style='%'
    )

    level = logging._nameToLevel.get(level, 'INFO')

    logger_ = logging.getLogger(app_name)
    logger_.setLevel(level)

    logger_.addHandler(TqdmLoggingHandler())

    if logger_.hasHandlers():
        for handlers in logger_.handlers:
            handlers.setFormatter(color_formatter)
            handlers.setLevel(level)

    return logger_

class GTComparisionError(Exception):
    """Raised when a comparison between a reference and an output has failed"""
    pass

class InconsistentDataError(Exception):
    """Raised when a json contains inconsistent data"""
    pass