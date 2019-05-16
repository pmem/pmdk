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

"""Base tests class and its functionalities"""

import builtins
import itertools
import shutil
import subprocess as sp
import sys
import re
import os
from datetime import datetime
from os import path

import context as ctx
import helpers as hlp
import valgrind as vg
from utils import Skip, Fail, fail


if not hasattr(builtins, 'testcases'):
    builtins.testcases = []


def get_testcases():
    """"Get list of testcases imported from src/test tree"""
    return builtins.testcases


# test case attributes that refer to selected context classes, their
# respective config field names and context base classes
CTX_COMPONENTS = (
    ('build', ctx._Build),
    ('fs', ctx._Fs)
)


class Any:
    """
    Test context attribute signifying that specific context value is not
    relevant for the test outcome and it should be run only once in some
    viable context
    """
    @classmethod
    def get(cls, conf_ctx):
        """Get specific context value to be run"""
        for c in conf_ctx:
            if c.is_preferred:
                # pick preferred if found
                return c
        # if no preferred is found, pick the first one
        return conf_ctx[0]


class _TestCase(type):
    """Metaclass for BaseTest that is used for registering imported tests"""

    def __init__(cls, name, bases, dct):
        type.__init__(cls, name, bases, dct)

        # only classes deriving from BaseTest are meant to be used
        if cls.__base__.__name__ != 'BaseTest':
            return

        # globally register class as test case
        builtins.testcases.append(cls)

        # expand values of context classes attributes
        for attr, _ in CTX_COMPONENTS:
            if hasattr(cls, attr):
                val = getattr(cls, attr)
                if isinstance(val, list):
                    val = ctx.expand(*val)
                setattr(cls, attr, val)

        cls.name = cls.__name__
        try:
            cls.testnum = int(cls.__name__.replace('TEST', ''))
        except ValueError as e:
            print('Invalid test class name {}, should be "TEST[number]"'
                  .format(cls.__name__))
            raise e


class BaseTest(metaclass=_TestCase):
    """Every test case need to inherit from this class"""
    test_type = ctx.Medium
    memcheck, pmemcheck, drd, helgrind = vg.AUTO, vg.AUTO, vg.AUTO, vg.AUTO
    valgrind = None
    memcheck_check_leaks = True
    match = True
    ld_preload = ''

    def __repr__(self):
        return '{}/{}'.format(self.group, self.__class__.__name__)

    def __init__(self, config):
        if self.__module__ == '__main__':
            self.cwd = path.dirname(path.abspath(sys.argv[0]))
        else:
            self.cwd = self.__module__

        self.config = config
        self.msg = hlp.Message(config)
        self.group = path.basename(self.cwd)

        self.testdir = self.group + '_' + str(self.testnum)
        self.utenv = self._get_utenv()
        self.ctxs = self._get_contexts()

    def _ctx_attrs_init(self):
        """
        Initialize test class attributes referring to selected context
        parameters (like build, fs). If attribute was not set by subclassing
        test, its value is set to respective config value. If it was set,
        it is filtered through config values.
        """

        for attr, base in CTX_COMPONENTS:
            conf_val = getattr(self.config, attr)
            if hasattr(self, attr):
                test_val = getattr(self, attr)
                if test_val == Any:
                    ctx_val = Any.get(conf_val)
                else:
                    ctx_val = hlp.filter_contexts(conf_val, test_val)
            else:
                ctx_val = hlp.filter_contexts(conf_val, None)
            setattr(self, attr, base.factory(self.config, *ctx_val))

        self._valgrind_init()

    def _valgrind_init(self):
        if sys.platform == 'win32':
            return

        vg_tool = vg.enabled_tool(self)

        if self.config.force_enable:
            if self.config.force_enable not in vg.disabled_tools(self):
                vg_tool = self.config.force_enable
            else:
                vg_tool = None

        self.valgrind = vg.Valgrind(vg_tool, self.cwd, self.testnum)

    def _get_contexts(self):
        """Initialize context classes used for each test run"""
        self._ctx_attrs_init()

        ctxs = []
        if self.test_type not in self.config.test_type:
            return ctxs

        # generate combination sets of context components
        ctx_sets = itertools.product(self.fs, self.build)
        for cs in ctx_sets:
            ctxs.append(ctx.Context(self, self.config, fs=cs[0], build=cs[1],
                                    valgrind=self.valgrind))

        return ctxs

    def _get_utenv(self):
        """Get environment variables values used by C test framework"""
        return {
            'UNITTEST_NAME': str(self),
            'UNITTEST_LOG_LEVEL': str(self.config.unittest_log_level),
            'UNITTEST_NUM': str(self.testnum)
        }

    def _execute(self):
        """
        Execute test for each context, return 1 if at least
        one test failed
        """
        failed = False
        for c in self.ctxs:
            config_str = '{}/{}/{}'.format(self.test_type, c.fs, c.build)
            if self.valgrind:
                config_str = '{}/{}'.format(config_str, self.valgrind)

            try:
                if self.valgrind:
                    self.valgrind.verify()

                self.msg.print('{}: SETUP\t({})'.format(self, config_str))
                # removes old log files, to make sure that logs made by test
                # are up to date
                self.remove_log_files()

                self.setup(c)
                start_time = datetime.now()
                self.run(c)
                self.elapsed = (datetime.now() - start_time).total_seconds()
                self.check()

            except Fail as f:
                failed = True
                print(f)
                self._print_log_files(c)
                print('{}: {}FAILED{}\t({})'
                      .format(self, hlp.Color.RED, hlp.Color.END, config_str))
                if not self.config.keep_going:
                    sys.exit(1)

            except Skip as s:
                print('{}: SKIP {}'.format(self, s))
                self.clean(c)

            except sp.TimeoutExpired:
                failed = True
                print('{}: {}TIMEOUT{}\t({})'
                      .format(self, hlp.Color.RED, hlp.Color.END, config_str))
                if not self.config.keep_going:
                    sys.exit(1)

            else:
                self._test_passed()
                self.clean(c)

        if failed:
            return 1
        return 0

    def setup(self, ctx):
        """Test setup"""
        if not path.exists(ctx.testdir):
            os.makedirs(ctx.testdir)

    def run(self, ctx):
        """
        Main test body, run with specific context provided through
        Context class instance. Needs to be implemented by each test
        """
        raise NotImplementedError('{} does not implement run() method'.format(
            self.__class__))

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
        match_cmd = prefix + path.join(hlp.ROOTDIR, 'match')

        for mf in match_files:
            cmd = '{} {}'.format(match_cmd, mf)
            proc = sp.run(cmd.split(), stdout=sp.PIPE, cwd=self.cwd,
                          stderr=sp.STDOUT, universal_newlines=True)
            if proc.returncode != 0:
                fail(proc.stdout, exit_code=proc.returncode)
            else:
                self.msg.print_verbose(proc.stdout)

    def clean(self, ctx):
        """Remove directory, even if it is not empty"""
        shutil.rmtree(ctx.testdir, ignore_errors=True)

    def _test_passed(self):
        """Print message specific for passed test"""
        if self.config.tm:
            tm = '\t\t\t[{:06.3F} s]'.format(self.elapsed)
        else:
            tm = ''

        self.msg.print('{}: {}PASS{} {}'
                       .format(self, hlp.Color.GREEN, hlp.Color.END, tm))

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

    def _print_log_files(self, ctx):
        """
        Prints all log files for given test
        """
        log_files = self.get_log_files()
        for file in log_files:
            with open(file) as f:
                ctx.dump_n_lines(f)

    def remove_log_files(self):
        """
        Removes log files for given test
        """
        log_files = self.get_log_files()
        for file in log_files:
            os.remove(file)
