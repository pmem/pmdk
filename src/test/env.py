#!/usr/bin/env python3
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
"""
Interpreter managing test group specific TESTS.py file execution.
It parses test classes from interpreted file, handles command line arguments
and executes tests using provided configuration.
"""


import importlib.util as importutil
import os
import sys

from testframework import BaseTest, Configurator, run_tests_common


def run_testcases():
    """Parse user configuration, run test cases"""
    config = Configurator().parse_config()
    testcases = BaseTest.__subclasses__()
    return run_tests_common(testcases, config)


def main():
    # Interpreter receives TESTS.py file as first argument
    if len(sys.argv) < 2:
        sys.exit('Provide test file to run')
    testfile = sys.argv[1]

    # Remove TESTS.py file from args, the rest of the args is parsed as a
    # test configuration
    sys.argv.pop(1)

    # import TESTS.py as a module
    testfile_dir = os.path.abspath(os.path.dirname(testfile))
    spec = importutil.spec_from_file_location(testfile_dir, testfile)
    module = importutil.module_from_spec(spec)
    spec.loader.exec_module(module)

    sys.exit(run_testcases())


if __name__ == '__main__':
    main()
