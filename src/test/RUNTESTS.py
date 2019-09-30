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

"""Main script for unit tests execution"""

import sys
import os
from os import path
sys.path.insert(1, path.abspath(path.join(path.dirname(__file__), 'unittest')))

# flake8 issues silenced:
# E402 - import statements not at the top of the file because of adding
# directory to path

import importlib.util as importutil  # noqa E402

import futils  # noqa E402
from basetest import get_testcases  # noqa E402
from configurator import Configurator  # noqa E402


def _import_testfiles():
    """
    Traverse through "src/test" directory, find all "TESTS.py" files and
    import them as modules. Set imported module name to file directory path.
    """
    for root, _, files in os.walk(futils.ROOTDIR):
        for name in files:
            if name == 'TESTS.py':
                testfile = path.join(root, name)
                spec = importutil.spec_from_file_location(
                    path.dirname(testfile), testfile)
                module = importutil.module_from_spec(spec)
                spec.loader.exec_module(module)


def _run_tests(config):
    """Run tests imported from repository tree from selected groups"""
    testcases = get_testcases()

    if config.group:
        # filter selected groups
        testcases = [t for t in testcases
                     if path.basename(t.__module__) in config.group]

        if not testcases:
            sys.exit('No testcases found for selected groups. '
                     'Provided group name may be invalid.')

    return futils.run_tests_common(testcases, config)


def main():
    config = Configurator().parse_config()
    _import_testfiles()
    sys.exit(_run_tests(config))


if __name__ == "__main__":
    main()
