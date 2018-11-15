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

"""Base tests class and its functionalities"""

import builtins
import itertools
import shutil
import subprocess as sp
import sys
from datetime import datetime
from os import listdir, makedirs, path

import context as ctx
import helpers as hlp


if not hasattr(builtins, 'testcases'):
    builtins.testcases = []


def get_testcases():
    return builtins.testcases


# test case attributes that refer to selected context classes, their
# respective config field names and context base classes
CTX_COMPONENTS = (
    ('builds', 'build', ctx._Build),
    ('fs', 'fs', ctx._Fs)
)


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
        for attr, _, _ in CTX_COMPONENTS:
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
    match = True

    def __repr__(self):
        return '{}/{}'.format(self.group, self.__class__.__name__)

    def __init__(self, config):
        if self.__module__ == '__main__':
            self.cwd = path.dirname(path.abspath(sys.argv[0]))
        else:
            self.cwd = self.__module__

        self.config = config
        self.msg = hlp.Message(config)
        self.failed, self.timeout = False, False
        self.group = path.basename(self.cwd)

        self.testdir = self.group + '_' + str(self.testnum)
        self.utenv = self._get_utenv()
        self.ctxs = self._get_contextes()

    def _ctx_attrs_init(self):
        """
        Initialize test class attributes referring to selected context
        parameters (like build, fs). If attribute was not set by subclassing
        test, its value is set to respective config value. If it was set,
        it is filtered through config values.
        """
        for test_attr, conf_attr, base in CTX_COMPONENTS:
            conf_val = getattr(self.config, conf_attr)
            if hasattr(self, test_attr):
                test_val = getattr(self, test_attr)
                test_val = [c for c in test_val if c.should_run(conf_val)]
            else:
                test_val = getattr(self.config, conf_attr)
            setattr(self, test_attr, base.factory(self.config, test_val))

    def _get_contextes(self):
        """Initialize context classes used for each test run"""
        self._ctx_attrs_init()

        ctxs = []
        if self.test_type not in self.config.test_type:
            return ctxs

        # generate combination sets of context components
        ctx_sets = itertools.product(self.fs, self.builds)
        for cs in ctx_sets:
            ctxs.append(ctx.Context(self, self.config, fs=cs[0], build=cs[1]))

        return ctxs

    def _get_utenv(self):
        """Get environment variables values used by C test framework"""
        return {
            'UNITTEST_NAME': str(self),
            'UNITTEST_LOG_LEVEL': str(self.config.unittest_log_level),
            'UNITTEST_NUM': str(self.testnum)
        }

    def _execute(self):
        """Execute test for each context"""
        for ctx in self.ctxs:
            self.failed, self.timeout = False, False
            print('{}: SETUP ({}/{}/{})'
                  .format(self, self.test_type, ctx.fs, ctx.build))

            self.setup(ctx)

            start_time = datetime.now()
            self.run(ctx)
            self.elapsed = (datetime.now() - start_time).total_seconds()

            self.check(ctx)
            self.clean(ctx)
            if self.failed and not self.config.keep_going:
                sys.exit(1)

    def setup(self, ctx):
        """Test setup"""
        if not path.exists(ctx.testdir):
            makedirs(ctx.testdir)

    def run(self, ctx):
        """
        Main test body, run with specific context provided through
        Context class instance. Needs to be implemented by each test
        """
        raise NotImplementedError('{} does not implement run() method'.format(
            self.__class__))

    def check(self, ctx):
        """ Determine test result """
        if self.failed:
            return self._fail()

        if self.match:
            self._run_match()

        return self._test_passed(ctx)

    def _fail(self, text=''):
        """Print fail message"""
        self.failed = True
        if text:
            print(text)
        self.msg.fail()

    def _run_match(self):
        """Match log files"""
        cwd_listdir = [path.join(self.cwd, f) for f in listdir(self.cwd)]

        suffix = '{}.log.match'.format(self.testnum)

        def is_matchfile(f):
            """Match file ends with specific suffix and a char before suffix
            is not a digit"""
            return path.isfile(f) and f.endswith(suffix) and \
            not f[-len(suffix) - 1].isdigit()

        match_files = filter(is_matchfile, cwd_listdir)
        prefix = 'perl ' if sys.platform == 'win32' else ''
        match_cmd = prefix + path.join(hlp.ROOTDIR, 'match')

        for mf in match_files:
            cmd = '{} {}'.format(match_cmd, mf)
            proc= sp.run(cmd.split(), stdout=sp.PIPE, cwd=self.cwd,
                          stderr=sp.STDOUT, universal_newlines=True)
            if proc.returncode != 0:
                self._fail(proc.stdout)
                return


    def clean(self, ctx):
        """ Remove directory, even if it is not empty. """
        shutil.rmtree(ctx.testdir, ignore_errors=True)

    def _test_passed(self, ctx):
        """ Print message specific for passed test """
        if self.config.tm:
            tm = '\t\t\t[{:06.3F} s]'.format(self.elapsed)
        else:
            tm = ''

        self.msg.print('{}: {}PASS {} {}'
                       .format(self, hlp.Color.GREEN, hlp.Color.END, tm))
