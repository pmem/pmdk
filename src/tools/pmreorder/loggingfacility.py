# Copyright 2018, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
