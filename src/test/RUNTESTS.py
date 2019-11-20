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
from ctx_filter import CtxFilter  # noqa E402


class TestRunner:
    def __init__(self, config, testcases):
        self.testcases = testcases
        self.config = config
        if self.config.test_sequence:
            # filter test cases from sequence
            self.testcases = [t for t in self.testcases
                              if t.testnum in self.config.test_sequence]

            # sort testcases so their sequence matches provided test sequence
            self.testcases.sort(key=lambda
                                tc: config.test_sequence.index(tc.testnum))

        if not self.testcases:
            sys.exit('No testcases to run found for selected configuration.')

        self.msg = futils.Message(config.unittest_log_level)

    def run_tests(self):
        """Run selected testcases"""
        ret = 0
        for tc in self.testcases:

            # TODO handle test type inside custom decorator
            if tc.test_type not in self.config.test_type:
                continue

            if not tc.enabled:
                continue

            cf = CtxFilter(self.config, tc)

            try:
                for c in cf.get_contexts():
                    try:
                        t = tc()
                        if t.enabled:
                            self.msg.print('{}: SETUP\t({}/{})'
                                           .format(t, t.test_type, c))
                            t._execute(c)
                        else:
                            continue

                    except futils.Skip as s:
                        self.msg.print_verbose('{}: SKIP: {}'.format(t, s))

                    except futils.Fail as f:
                        self._test_failed(t, c, f)
                        ret = 1
                    else:
                        self._test_passed(t)

            except futils.Skip as s:
                self.msg.print_verbose('{}: SKIP: {}'.format(tc, s))

        return ret

    def _test_failed(self, tc, ctx, fail):
        self.msg.print(fail)
        self.msg.print('{}: {}FAILED{}\t({}/{})'
                       .format(tc, futils.Color.RED,
                               futils.Color.END, tc.test_type, ctx))

        if not self.config.keep_going:
            sys.exit(1)

    def _test_passed(self, tc):
        """Print message specific for passed test"""
        if self.config.tm:
            tm = '\t\t\t[{:06.3F} s]'.format(tc.elapsed)
        else:
            tm = ''

        self.msg.print('{}: {}PASS{} {}'
                       .format(tc, futils.Color.GREEN, futils.Color.END, tm))


def _import_testfiles():
    """
    Traverse through "src/test" directory, find all "TESTS.py" files and
    import them as modules. Set imported module name to
    file directory path.
    """
    for root, _, files in os.walk(futils.ROOTDIR):
        for name in files:
            if name == 'TESTS.py':
                testfile = path.join(root, name)
                spec = importutil.spec_from_file_location(
                    path.dirname(testfile), testfile)
                module = importutil.module_from_spec(spec)
                spec.loader.exec_module(module)


def main():
    _import_testfiles()
    config = Configurator().config
    testcases = get_testcases()

    if config.group:
        # filter selected groups
        testcases = [t for t in testcases
                     if path.basename(t.__module__) in config.group]

    runner = TestRunner(config, testcases)
    sys.exit(runner.run_tests())


if __name__ == "__main__":
    main()
