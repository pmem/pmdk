#
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
#

"""Helper variables, functions and classes"""

from os.path import join, abspath, dirname
import sys


# Constant paths repository elements
ROOTDIR = abspath(join(dirname(__file__), '..'))

WIN_DEBUG_BUILDDIR = abspath(join(ROOTDIR, '..', 'x64', 'Debug'))
WIN_DEBUG_EXEDIR = abspath(join(WIN_DEBUG_BUILDDIR, 'tests'))

WIN_NONDEBUG_BUILDDIR = abspath(join(ROOTDIR, '..', 'x64', 'Release'))
WIN_NONDEBUG_EXEDIR = abspath(join(WIN_NONDEBUG_BUILDDIR, 'tests'))

if sys.platform == 'win32':
    DEBUG_LIBDIR = abspath(join(WIN_DEBUG_BUILDDIR, 'libs'))
    NONDEBUG_LIBDIR = abspath(join(WIN_NONDEBUG_BUILDDIR, 'libs'))
else:
    DEBUG_LIBDIR = abspath(join(ROOTDIR, '..', 'debug'))
    NONDEBUG_LIBDIR = abspath(join(ROOTDIR, '..', 'nondebug'))

#
# KB, MB, GB, TB, PB -- these functions convert the integer to bytes
#
# example:
#   MB(3)  -->  3145728
#   KB(16) -->  16384
#


def KB(n):
    return 2 ** 10 * n


def MB(n):
    return 2 ** 20 * n


def GB(n):
    return 2 ** 30 * n


def TB(n):
    return 2 ** 40 * n


def PB(n):
    return 2 ** 50 * n


class Color:
    """This class sets the font color"""
    RED = '\33[91m'
    GREEN = '\33[92m'
    END = '\33[0m'


class Message:
    """ This class checks the value of unittest_log_level
        to print the message. """

    def __init__(self, config):
        self.config = config

    def fail(self):
        print('{}FAILED {}'.format(Color.RED, Color.END))

    def print(self, msg):
        if self.config.unittest_log_level >= 1:
            print(msg)

    def print_verbose(self, msg):
        if self.config.unittest_log_level >= 2:
            print(msg)
