# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2023, Intel Corporation

"""Base tests class and its functionalities.

_TestCase metaclass implemented in this module is
used to register test case classes.

Classes are registered and used as actual test cases only if they fulfill
the following requirements:
    - inherit (directly or indirectly) from BaseTest class
    - reside in TESTS.py file under pmdk/src/test/ directory
    - are named by TEST[int] (e.g. TEST0), where integer suffix is a
      test number (used for selecting test sequences).

BaseTest._execute() method requires special attention as a
general single test execution workflow.

While BaseTest class is the required test case base class, it consist only of
the minimal implementation to serve as the test framework handle.
In practice, it is the Test class (inheriting from BaseTest) that is
most commonly used as a test case direct base class. The Test class
implements operations typically used by most of the tests.

Test class needs to implement an abstract run(ctx) method, which stands
for a test body. setup(ctx) and clean(ctx) methods, which are a part
of test execution workflow are optional. Mind that the Test class already
provides some implementation of the setup() and clean() - it is
crucial to call them while implementing own setup() and clean() methods.

Tests for PMDK C code frequently call the Context exec() method
(passed as 'ctx' argument to run()), which is a utility for
setting up, running and handling the output of C test binaries.

Example TESTS.py file with a single test case may look like this:

$ cat pmdk/src/test/test_group/TESTS.py

!#../env.py

import testframework as t


class TEST0(t.Test):
    def run(self, ctx):
        ctx.exec('test_binary')

    def setup(self, ctx):
        # call Test setup
        super().setup(ctx)
        # TEST0 setup implementation goes here

    def clean(self, ctx):
        super().clean(ctx)
        # TEST0 clean implementation goes here

"""

import builtins
import subprocess as sp
import sys
import re
import os
from datetime import datetime
from os import path

from configurator import Configurator
from consts import LIBS_LIST, ROOTDIR
import futils
import test_types
import shutil


#
# imported test cases are globally available as a 'builtins'
# module 'testcases' variable, which is initialized at first basetest
# module import as an empty list
#
if not hasattr(builtins, 'testcases'):
    builtins.testcases = []


def get_testcases():
    """"Get list of testcases imported from src/test tree"""
    return builtins.testcases


def _test_string_repr(cls):
    """
    Implementation of __str__ method for the test class. Needs to be available
    both for initialized (as a BaseTest instance method)
    as well as uninitialized object (as a _TestCase metaclass method)
    """
    return '{}/{}'.format(cls.group, cls.name)


class _TestCase(type):
    """Metaclass for BaseTest that is used for registering imported tests.

    Attributes:
        cwd (str): path to the directory of the TESTS.py file containing the
            class
        group (str): name of the directory of the TESTS.py file
        testnum (int): test number
        tc_dirname (str): name of the directory created for the test in testdir
    """

    def __init__(cls, name, bases, dct):
        type.__init__(cls, name, bases, dct)

        # globally register class as test case
        # only classes whose names start with 'TEST' are meant to be run
        cls.name = cls.__name__
        if cls.__module__ == '__main__':
            cls.cwd = path.dirname(path.abspath(sys.argv[0]))
        else:
            cls.cwd = cls.__module__

        cls.group = path.basename(cls.cwd)

        if cls.name.startswith('TEST'):
            builtins.testcases.append(cls)
            try:
                cls.testnum = int(cls.name.replace('TEST', ''))
            except ValueError as e:
                print('Invalid test class name {}, should be "TEST[number]"'
                      .format(cls.name))
                raise e

            cls.tc_dirname = cls.group + '_' + str(cls.testnum)

    def __str__(cls):
        return _test_string_repr(cls)


class BaseTest(metaclass=_TestCase):
    """
    Framework base test class. Every test case needs to (directly or
    indirectly) inherit from it. Since this class implements only
    very abstract test behaviour, it is advised for most test cases
    to use Test class inheriting from it.

    Attributes:
        enabled (bool): True if test is meant to be executed, False otherwise.
            Defaults to True.
        ctx (Context): context affiliated with the test
    """
    enabled = True

    def __init__(self):
        self.ctx = None

    def __str__(self):
        return _test_string_repr(self)

    def _execute(self, c):
        """
        Implementation of basic single contextualized test execution workflow.
        Called by the test runner.

        Args:
            c (Context): test context
        """
        self.ctx = c

        try:
            # pre-execution cleanup
            start_time = datetime.now()
            self.ctx.clean()
            self.clean()

            self.ctx.setup()
            self.setup(c)

            start_time = datetime.now()
            self.run(c)
            self.elapsed = (datetime.now() - start_time).total_seconds()

            self.ctx.check()
            self.check(c)

        except futils.Fail:
            if start_time is not None:
                self.elapsed = (datetime.now() - start_time).total_seconds()
            self._on_fail()
            raise

        except futils.Skip:
            self.ctx.clean()
            self.clean()
            raise

        except sp.TimeoutExpired:
            msg = '{}: {}TIMEOUT{}\t({})'.format(self, futils.Color.RED,
                                                 futils.Color.END,
                                                 self.ctx)
            if start_time is not None:
                self.elapsed = (datetime.now() - start_time).total_seconds()
            raise futils.Fail(msg)

        else:
            self.ctx.clean()
            self.clean()

    def setup(self, ctx):
        """Test setup - not implemented by BaseTest"""
        pass

    def run(self, ctx):
        """
        Main test body, run with specific context provided through
        Context class instance. Needs to be implemented by each test
        """
        raise NotImplementedError('{} does not implement run() method'.format(
            self.__class__))

    def check(self, ctx):
        """Run additional test checks - not implemented by BaseTest"""
        pass

    def clean(self):
        """Test cleanup - not implemented by BaseTest"""
        pass

    def _on_fail(self):
        """Custom behaviour on test fail - not implemented by BaseTest"""
        pass


class Test(BaseTest):
    """
    Generic implementation of BaseTest scaffolding used by particular test
    case classes as a base. In practice, most tests need to inherit from
    this class rather than from Basetest since it implements most commonly
    used test utilities and actions.

    Args:
        test_type (_TestType): test type affiliated with test execution
            length. Defaults to Medium.
        match (bool): True if match files will be checked for test,
            False otherwise. Defaults to True.
        config (Configurator): test execution configuration
        msg (Message): message class instance with logging level
        env (dict): environment variables needed for test execution

    """
    test_type = test_types.Medium
    match = True

    def __init__(self):
        super().__init__()
        self.config = Configurator().config
        self.msg = futils.Message(self.config.unittest_log_level)
        self.log_files = {}

    def _get_utenv(self):
        """Get environment variables values used by C test framework"""
        return {
            'UNITTEST_NAME': str(self),
            'UNITTEST_LOG_LEVEL': str(self.config.unittest_log_level),
            'UNITTEST_NUM': str(self.testnum)
        }

    def _debug_log_env(self):
        """
        Return environment variables that enable logging PMDK debug output
        into log files
        """

        envs = {}
        for libs in LIBS_LIST:
            envs['{}_LOG_LEVEL'.format(libs.upper())] = '3'
            log_file = os.path.join(self.cwd,
                                    '{}_{}.log'.format(libs, self.testnum))
            envs['{}_LOG_FILE'.format(libs.upper())] = log_file
            self.log_files[libs] = log_file
        return envs

    def get_log_files(self):
        """
        Returns names of all log files for given test
        """
        pattern = r'.*[a-zA-Z_]{}\.log'
        log_files = []
        files = os.scandir(self.cwd)
        for file in files:
            match = re.fullmatch(pattern.format(self.testnum), file.name)
            if match:
                log = path.abspath(path.join(self.cwd, file.name))
                log_files.append(log)
        return log_files

    def get_log_file_by_prefix(self, prefix):
        """
        Returns path of a log file with specific prefix and number
        corresponding to self.testnum
        """
        return next(filter(
            lambda x: os.path.basename(x).startswith(
                F"{prefix}_{self.testnum}"), self.get_log_files()
        ))

    def _print_log_files(self):
        """
        Prints all log files for given test
        """
        log_files = self.get_log_files()
        for file in log_files:
            with open(file) as f:
                self.ctx.dump_n_lines(f)

    def _move_log_files(self, ctx):
        """
        Move all log files for given tests
        """
        path = "logs"
        sub_dir = str(ctx).replace(':', '')
        logs_dir = os.path.join(path, sub_dir)
        os.makedirs(logs_dir, exist_ok=True)
        log_files = self.get_log_files()
        for file in log_files:
            shutil.copy2(file, logs_dir)

    def remove_log_files(self):
        """
        Removes log files for given test
        """
        log_files = self.get_log_files()
        for file in log_files:
            os.remove(file)

    def setup(self, ctx):
        """Test setup"""
        self.env = {}
        self.env.update(self._get_utenv())
        self.env.update(self._debug_log_env())
        self.ctx.add_env(self.env)

        self.remove_log_files()

    def _on_fail(self):
        self._print_log_files()

    def check(self, ctx):
        """Run additional test checks"""
        if self.match:
            self._run_match()
        self._move_log_files(ctx)

    def _run_match(self):
        """Match log files"""
        cwd_listdir = [path.join(self.cwd, f) for f in os.listdir(self.cwd)]

        suffix = '{}.log.match'.format(self.testnum)

        def is_matchfile(f):
            """Match file ends with specific suffix and a char before suffix
            is not a digit"""
            before_suffix = -len(suffix) - 1
            return path.isfile(f) and f.endswith(suffix) and \
                not f[before_suffix].isdigit()

        match_files = filter(is_matchfile, cwd_listdir)
        match_cmd = path.join(ROOTDIR, 'match')

        for mf in match_files:
            cmd = '{} {}'.format(match_cmd, mf)
            proc = sp.run(cmd.split(), stdout=sp.PIPE, cwd=self.cwd,
                          stderr=sp.STDOUT, universal_newlines=True)
            if proc.returncode != 0:
                futils.fail(proc.stdout, exit_code=proc.returncode)
            else:
                self.msg.print_verbose(proc.stdout)
