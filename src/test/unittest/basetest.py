# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation

"""Base tests class and its functionalities"""

import builtins
import subprocess as sp
import sys
import re
import os
from datetime import datetime
from os import path

from configurator import Configurator
import futils
import test_types


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
    """Metaclass for BaseTest that is used for registering imported tests"""

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
    indirectly) inherit from this class. Since this class implements only
    very abstract test behaviour, it is advised for particular test cases
    to use Test class inheriting from it.
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
        """
        self.ctx = c

        try:
            # pre-execution cleanup
            self.ctx.clean()
            self.clean()

            self.ctx.setup()
            self.setup()

            start_time = datetime.now()
            self.run(c)
            self.elapsed = (datetime.now() - start_time).total_seconds()

            self.ctx.check()
            self.check()

        except futils.Fail:
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
            raise futils.Fail(msg)

        else:
            self.ctx.clean()
            self.clean()

    def setup(self):
        """Test setup - not implemented by BaseTest"""
        pass

    def run(self, ctx):
        """
        Main test body, run with specific context provided through
        Context class instance. Needs to be implemented by each test
        """
        raise NotImplementedError('{} does not implement run() method'.format(
            self.__class__))

    def check(self):
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
    case classes as a base.
    """
    test_type = test_types.Medium
    memcheck_check_leaks = True
    match = True

    def __init__(self):
        super().__init__()
        self.config = Configurator().config
        self.msg = futils.Message(self.config.unittest_log_level)

    def _get_utenv(self):
        """Get environment variables values used by C test framework"""
        return {
            'UNITTEST_NAME': str(self),
            'UNITTEST_LOG_LEVEL': str(self.config.unittest_log_level),
            'UNITTEST_NUM': str(self.testnum)
        }

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

    def _print_log_files(self):
        """
        Prints all log files for given test
        """
        log_files = self.get_log_files()
        for file in log_files:
            with open(file) as f:
                self.ctx.dump_n_lines(f)

    def remove_log_files(self):
        """
        Removes log files for given test
        """
        log_files = self.get_log_files()
        for file in log_files:
            os.remove(file)

    def setup(self):
        """Test setup"""
        self.env = {}
        self.env.update(self._get_utenv())
        self.ctx.add_env(self.env)

        self.remove_log_files()

    def _on_fail(self):
        self._print_log_files()

    def check(self):
        """Run additional test checks"""
        if self.match:
            self._run_match()

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
        prefix = 'perl ' if sys.platform == 'win32' else ''
        match_cmd = prefix + path.join(futils.ROOTDIR, 'match')

        for mf in match_files:
            cmd = '{} {}'.format(match_cmd, mf)
            proc = sp.run(cmd.split(), stdout=sp.PIPE, cwd=self.cwd,
                          stderr=sp.STDOUT, universal_newlines=True)
            if proc.returncode != 0:
                futils.fail(proc.stdout, exit_code=proc.returncode)
            else:
                self.msg.print_verbose(proc.stdout)
