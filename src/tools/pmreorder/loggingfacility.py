# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Intel Corporation

import logging

log_levels = ["debug", "info", "warning", "error", "critical"]


class LoggingBase:
    def debug(self, text):
        pass

    def info(self, text):
        pass

    def warning(self, text):
        pass

    def error(self, text):
        pass

    def critical(self, text):
        pass


class DefaultFileLogger(LoggingBase):
    def __init__(self, name="pmreorder", **kwargs):
        logging.basicConfig(**kwargs)
        self.__logger = logging.getLogger(name)

    def debug(self, text):
        self.__logger.debug(text)

    def info(self, text):
        self.__logger.info(text)

    def warning(self, text):
        self.__logger.warning(text)

    def error(self, text):
        self.__logger.error(text)

    def critical(self, text):
        self.__logger.critical(text)


class DefaultPrintLogger(LoggingBase):

    def debug(self, text):
        print("DEBUG:", text)

    def info(self, text):
        print("INFO:", text)

    def warning(self, text):
        print("WARNING:", text)

    def error(self, text):
        print("ERROR:", text)

    def critical(self, text):
        print("CRITICAL:", text)


def get_logger(log_output, log_level=None):
    logger = None
    # check if log_level is valid
    log_level = "warning" if log_level is None else log_level
    numeric_level = getattr(logging, log_level.upper())
    if not isinstance(numeric_level, int):
        raise ValueError('Invalid log level: %s'.format(log_level.upper()))

    if log_output is None:
        logger = DefaultPrintLogger()
    else:
        logger = DefaultFileLogger(filename=log_output, level=numeric_level)
    return logger
