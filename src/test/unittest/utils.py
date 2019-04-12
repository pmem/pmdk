#
# Copyright 2019, Intel Corporation
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
#

"""Set of basic functions available to use in all test cases"""


import sys
import shutil

import tools
from helpers import MiB, KiB


try:
    import testconfig
except ImportError:
    sys.exit('Please add valid testconfig.py file - see testconfig.py.example')
config = testconfig.config

HEADER_SIZE = 4096


class Fail(Exception):
    """Thrown when test fails"""

    def __init__(self, message):
        super().__init__(message)
        self.message = message

    def __str__(self):
        return self.message


def fail(msg, exit_code=None):
    if exit_code is not None:
        msg = '{}\nError {}'.format(msg, exit_code)
    raise Fail(msg)


class Skip(Exception):
    """Thrown when test should be skipped"""

    def __init__(self, message):
        super().__init__(message)
        self.message = message

    def __str__(self):
        return self.message


def skip(msg):
    raise Skip(msg)


def dump_n_lines(file, n=None):
    """
    Prints last n lines of given log file. Number of line printed can be
    modified locally by "n" argument or globally by "dump_lines" in
    testconfig.py file. If none of this is provided, default value is 30.
    """
    if n is None:
        n = config.get('dump_lines', 30)

    file_size = get_size(file.name)
    if file_size < 100 * MiB:
        lines = list(file)
        length = len(lines)
        if n > length:
            n = length
        lines = lines[-n:]
        lines.insert(0, 'Last {} lines of {} below (whole file has {} lines).'
                        ''.format(n, file.name, length))
        for line in lines:
            print(line, end='')
    else:
        with open(file.name, 'br') as byte_file:
            byte_file.seek(file_size - 10 * KiB)
            print(byte_file.read().decode('iso_8859_1'))


def is_devdax(path):
    """Checks if given path points to device dax"""
    proc = tools.pmemdetect('-d', path)
    if proc.returncode == tools.PMEMDETECT_ERROR:
        fail(proc.stdout)
    if proc.returncode == tools.PMEMDETECT_TRUE:
        return True
    if proc.returncode == tools.PMEMDETECT_FALSE:
        return False
    fail('Unknown value {} returned by pmemdetect'.format(proc.returncode))


def supports_map_sync(path):
    """Checks if MAP_SYNC is supported on a filesystem from given path"""
    proc = tools.pmemdetect('-s', path)
    if proc.returncode == tools.PMEMDETECT_ERROR:
        fail(proc.stdout)
    if proc.returncode == tools.PMEMDETECT_TRUE:
        return True
    if proc.returncode == tools.PMEMDETECT_FALSE:
        return False
    fail('Unknown value {} returned by pmemdetect'.format(proc.returncode))


def get_size(path):
    """
    Returns size of the file or dax device.
    Value "2**64 - 1" is checked because pmemdetect in case of error prints it.
    """
    proc = tools.pmemdetect('-z', path)
    if int(proc.stdout) != 2**64 - 1:
        return int(proc.stdout)
    fail('Could not get size of the file, it is inaccessible or does not exist')


def get_free_space():
    """Returns free space for current file system"""
    _, _, free = shutil.disk_usage(".")
    return free
