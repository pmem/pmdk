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

"""Test framework utilities"""

from os.path import join, abspath, dirname
import os
import sys

# Constant paths to repository elements
ROOTDIR = abspath(join(dirname(__file__), '..'))

WIN_DEBUG_BUILDDIR = abspath(join(ROOTDIR, '..', 'x64', 'Debug'))
WIN_DEBUG_EXEDIR = abspath(join(WIN_DEBUG_BUILDDIR, 'tests'))

WIN_RELEASE_BUILDDIR = abspath(join(ROOTDIR, '..', 'x64', 'Release'))
WIN_RELEASE_EXEDIR = abspath(join(WIN_RELEASE_BUILDDIR, 'tests'))

if sys.platform == 'win32':
    DEBUG_LIBDIR = abspath(join(WIN_DEBUG_BUILDDIR, 'libs'))
    RELEASE_LIBDIR = abspath(join(WIN_RELEASE_BUILDDIR, 'libs'))
else:
    DEBUG_LIBDIR = abspath(join(ROOTDIR, '..', 'debug'))
    RELEASE_LIBDIR = abspath(join(ROOTDIR, '..', 'nondebug'))


def get_tool_path(ctx, name):
    if sys.platform == 'win32':
        if str(ctx.build) == 'debug':
            return abspath(join(WIN_DEBUG_BUILDDIR, 'libs', name + '.exe'))
        else:
            return abspath(join(WIN_RELEASE_BUILDDIR, 'libs', name + '.exe'))
    else:
        return abspath(join(ROOTDIR, '..', 'tools', name, name))


def get_test_tool_path(ctx, name):
    if sys.platform == 'win32':
        if str(ctx.build) == 'debug':
            return abspath(join(WIN_DEBUG_BUILDDIR, 'tests', name + '.exe'))
        else:
            return abspath(join(WIN_RELEASE_BUILDDIR, 'tests', name + '.exe'))
    else:
        return abspath(join(ROOTDIR, 'tools', name, name))


def get_lib_dir(ctx):
    if str(ctx.build) == 'debug':
        return DEBUG_LIBDIR
    else:
        return RELEASE_LIBDIR


def get_examples_dir(ctx):
    if sys.platform == 'win32':
        if str(ctx.build) == 'debug':
            return abspath(join(WIN_DEBUG_BUILDDIR, 'examples'))
        else:
            return abspath(join(WIN_RELEASE_BUILDDIR, 'examples'))
    else:
        return abspath(join(ROOTDIR, '..', 'examples'))


class Color:
    """
    Set the font color. This functionality relies on ANSI espace sequences
    and is currently disabled for Windows
    """
    if sys.platform != 'win32':
        RED = '\33[91m'
        GREEN = '\33[92m'
        END = '\33[0m'
    else:
        RED, GREEN, END = '', '', ''


class Message:
    """Simple level based logger"""

    def __init__(self, config):
        self.config = config

    def print(self, msg):
        if self.config.unittest_log_level >= 1:
            print(msg)

    def print_verbose(self, msg):
        if self.config.unittest_log_level >= 2:
            print(msg)


def filter_contexts(config_ctx, test_ctx):
    """
    Return contexts that should be used in execution based on
    contexts provided by config and test case
    """
    if not test_ctx:
        return [c for c in config_ctx if not c.explicit]
    return [c for c in config_ctx if c in test_ctx]


def run_tests_common(testcases, config):
    """
    Common implementation for running tests - used by RUNTESTS.py and
    single test case interpreter
    """
    if config.test_sequence:
        # filter test cases from sequence
        testcases = [t for t in testcases if t.testnum in config.test_sequence]
        # sort testcases so their sequence matches provided test sequence
        testcases.sort(key=lambda tc: config.test_sequence.index(tc.testnum))

    if not testcases:
        sys.exit('No testcases to run found for selected configuration.')

    for t in testcases:
        try:
            t = t(config)
        except Skip as s:
            print(s)
        else:
            if t.enabled and t._execute():  # if test failed
                return 1
    return 0


class Fail(Exception):
    """Thrown when test fails"""

    def __init__(self, message):
        super().__init__(message)
        self.message = message

    def __str__(self):
        return self.message


def fail(msg, exit_code=None):
    if exit_code is not None:
        msg = '{}{}Error {}'.format(msg, os.linesep, exit_code)
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
