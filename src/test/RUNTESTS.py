#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2022, Intel Corporation

"""Main script for unit tests execution.

Contains the implementation of the TestRunner class, which is a main
test execution manager class and it's also used by TESTS.py scripts if run
individually.

TestRunner run_tests() method requires special attention
as it implements the fundamental test run workflow.

"""

# modules from unittest directory are visible from this script
import sys
import os
from os import path
sys.path.insert(1, path.abspath(path.join(path.dirname(__file__), 'unittest')))

# flake8 issues silenced:
# E402 - import statements not at the top of the file because of adding
# directory to path

import importlib.util as importutil  # noqa E402
import subprocess as sp  # noqa E402

import futils  # noqa E402
from consts import ROOTDIR # noqa E402
from basetest import get_testcases  # noqa E402
from configurator import Configurator  # noqa E402
from ctx_filter import CtxFilter  # noqa E402


class TestRunner:
    """
    Main script utility for managing tests execution.

    Attributes:
        testcases (list): BaseTest objects that are test
            cases to be run during execution.
        config: config object as returned by Configurator
        msg (Message) level based logger

    """
    def __init__(self, config, testcases):
        self.testcases = testcases
        self.config = config
        self._check_admin()
        self.msg = futils.Message(config.unittest_log_level)

        if self.config.test_sequence:
            # filter test cases from sequence
            self.testcases = [t for t in self.testcases
                              if t.testnum in self.config.test_sequence]

            # sort testcases so their sequence matches provided test sequence
            self.testcases.sort(key=lambda
                                tc: config.test_sequence.index(tc.testnum))

        if not self.testcases:
            sys.exit('No testcases to run found for selected configuration.')

    def _check_admin(self):
        if not self.config.enable_admin_tests:
            return
        if sys.platform != 'win32':
            """This check is valid only for linux OSes"""
            try:
                sp.check_output(['sudo', '-n', 'true'], stderr=sp.STDOUT)
            except sp.CalledProcessError:
                sys.exit('Enabled "enable_admin_tests" requires '
                         'the non-interactive sudo (no password required to '
                         'perform the sudo command).')
        """XXX add a similar check for Windows"""

    def run_tests(self):
        """Run selected testcases.

        Implementation of this method is crucial as a general
        tests execution workflow.

        Returns:
            main script exit code denoting the execution result.

        """
        ret = 0
        for tc in self.testcases:

            # TODO handle test type inside custom decorator
            if tc.test_type not in self.config.test_type:
                continue

            if not tc.enabled:
                continue

            cf = CtxFilter(self.config, tc)

            # The 'c' context has to be initialized before the 'for' loop,
            # because cf.get_contexts() can return no value ([])
            # and in case of the 'futils.Fail' exception
            # self._test_failed(tc, c, f) will be called
            # with uninitilized value of the 'c' context.
            c = None
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
                        self.msg.print('{}: SKIP: {}'.format(t, s))

                    except futils.Fail as f:
                        self._test_failed(t, c, f)
                        ret = 1
                    else:
                        self._test_passed(t)

            except futils.Skip as s:
                self.msg.print('{}: SKIP: {}'.format(tc, s))
            except futils.Fail as f:
                self._test_failed(tc, c, f)
                ret = 1

        return ret

    def _test_failed(self, tc, ctx, fail):
        self.msg.print('{}: {}FAILED{}\t({}/{})'
                       .format(tc, futils.Color.RED,
                               futils.Color.END, tc.test_type, ctx))
        self.msg.print(fail)

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

    Importing these files serves two purposes:
        - makes test classes residing in them visible
          and usable by the RUNTESTS script.
        - triggers basetest._TestType metaclass which initializes and
          registers them as actual test case classes - they are
          therefore available through basetest.get_testfiles() function

    """
    for root, _, files in os.walk(ROOTDIR):
        for name in files:
            if name == 'TESTS.py':
                testfile = path.join(root, name)
                module_name = path.dirname(testfile)
                spec = importutil.spec_from_file_location(module_name,
                                                          testfile)
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

    if hasattr(config, 'list_testcases'):
        for t in testcases:
            print(t.name)
        sys.exit(0)

    runner = TestRunner(config, testcases)
    sys.exit(runner.run_tests())


if __name__ == '__main__':
    main()
