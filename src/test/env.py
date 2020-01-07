#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation
#
"""
Interpreter managing test group specific TESTS.py file execution.
It parses test classes from interpreted file, handles command line arguments
and executes tests using provided configuration.
"""

import importlib.util as importutil
import os
import sys

from testframework import Configurator, get_testcases
from RUNTESTS import TestRunner


def run_testcases():
    """Parse user configuration, run test cases"""
    config = Configurator().config
    testcases = get_testcases()
    runner = TestRunner(config, testcases)
    return runner.run_tests()


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
