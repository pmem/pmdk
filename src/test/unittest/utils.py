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


import os
import sys

from helpers import MiB, KiB


try:
    import testconfig
except ImportError:
    sys.exit('Please add valid testconfig.py file - see testconfig.py.example')
config = testconfig.config


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


def get_size(path):
    """Returns size of the file, does not work with dax devices"""
    try:
        statinfo = os.stat(path)
        return statinfo.st_size
    except OSError as err:
        fail(err)
