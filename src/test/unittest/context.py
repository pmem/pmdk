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
"""Set of classes that represent the context of single test execution
(like build, filesystem, test type)"""

import os
import sys
import itertools
from datetime import datetime
import subprocess as sp
from collections import Iterable

from helpers import Color, Message


class Context:
    """Manage test execution based on values from context classes"""

    def __init__(self, test, conf, fs, build):
        self.proc, self.start_time, self.end_time = None, None, None
        self.test = test
        self.conf = conf
        self.build = build
        self.fs = fs
        self.testdir = os.path.join(fs.dir, test.testdir)

    def create_holey_file(self, size, name):
        """Ccreate a new file with the selected size and name"""
        filepath = os.path.join(self.testdir, name)
        with open(filepath, 'w') as f:
            f.seek(size)
            f.write('\0')
        return filepath

    def test_binary_exec(self, cmd, args=''):
        """Execute test C binary in current test context"""
        if sys.platform == 'win32':
            cmd = os.path.join(self.build.exedir, cmd) + '.exe'
        else:
            cmd = os.path.join(self.test.cwd, cmd) + self.build.exesuffix

        self.start_time = datetime.now()
        env = {**self.build.env, **self.test.utenv}
        try:
            self.proc = sp.run([cmd, args],
                               timeout=self.conf.test_timeout.total_seconds(),
                               env=env, stdout=sp.PIPE, cwd=self.test.cwd,
                               stderr=sp.STDOUT, universal_newlines=True)
        except sp.TimeoutExpired:
            self.test.timeout = True
            Message(self.conf).print('Skipping: {} {} timed out{}'.format(
                self.test.name, Color.RED, Color.END))

        finally:
            self.end_time = datetime.now()


class _Container(type):
    """Metaclass for context classes that can stand for mutliple classes"""
    def __init__(cls, name, bases, dct):
        type.__init__(cls, name, bases, dct)
        # It is supposed that context class has a user defined base class
        if object not in cls.__bases__:
            if not hasattr(cls, 'includes'):
                cls.includes = [cls]
            # remove duplications
            cls.includes = list(set(cls.includes))

    def __iter__(cls):
        for c in cls.includes:
            yield c


class _Build(metaclass=_Container):
    """Base and factory class for build classes"""

    def __repr__(self):
        return self.__class__.__name__.lower()

    @classmethod
    def factory(cls, conf, *classes):
        return [c(conf) for c in set(itertools.chain(*classes))]


class Debug(_Build):
    """This class sets the context for debug build"""

    def __init__(self, conf):
        self.env = os.environ.copy()
        self.exesuffix = ''
        if sys.platform == 'win32':
            builddir = os.path.abspath(os.path.join(
                conf.rootdir, '..', 'x64', 'Debug'))
            self.exedir = os.path.join(builddir, 'tests')
            self.env['PATH'] = self.env['PATH'] + \
                os.pathsep + os.path.join(builddir, 'libs')
        else:
            self.env['LD_LIBRARY_PATH'] = os.path.join(
                conf.rootdir, '..', 'debug')


class Nondebug(_Build):
    """This class sets the context for nondebug build"""

    def __init__(self, conf):
        self.env = os.environ.copy()
        self.exesuffix = ''

        if sys.platform == 'win32':
            builddir = os.path.abspath(os.path.join(
                conf.rootdir, '..', 'x64', 'Release'))
            self.exedir = os.path.join(builddir, 'tests')
            self.env['PATH'] = self.env['PATH'] + os.pathsep + os.path.join(
                builddir, 'libs')
        else:
            self.env['LD_LIBRARY_PATH'] = os.path.join(
                conf.rootdir, '..', 'nondebug')


# Build types not available on Windows
if sys.platform != 'win32':
    class Static_Debug(_Build):
        """Sets the context for static_debug build"""

        def __init__(self, conf):
            self.env = os.environ.copy()
            self.exesuffix = '.static-debug'
            self.env['LD_LIBRARY_PATH'] = os.path.join(
                conf.rootdir, '..', 'debug')

    class Static_Nondebug(_Build):
        """Sets the context for static_nondebug build"""

        def __init__(self, conf):
            self.env = os.environ.copy()
            self.exesuffix = '.static-nondebug'
            self.env['LD_LIBRARY_PATH'] = os.path.join(
                conf.rootdir, '..', 'nondebug')


class AllBuilds(_Build):
    if sys.platform == 'win32':
        includes = [Debug, Nondebug]
    else:
        includes = [Debug, Nondebug, Static_Debug, Static_Nondebug]


class _Fs(metaclass=_Container):
    """Base class and factory for filesystem classes"""

    def __repr__(self):
        return self.__class__.__name__.lower()

    @classmethod
    def factory(cls, conf, *classes):
        return [c(conf) for c in set(itertools.chain(*classes))]


class Pmem(_Fs):
    """This class sets the context for pmem filesystem"""

    def __init__(self, conf):
        self.dir = os.path.abspath(conf.pmem_fs_dir)


class Nonpmem(_Fs):
    """This class sets the context for nonpmem filesystem"""

    def __init__(self, conf):
        self.dir = conf.non_pmem_fs_dir


class NoFs(_Fs):
    def __init__(self, conf):
        pass


class AllFs(_Fs):
    includes = [Pmem, Nonpmem]


class AnyFs(_Fs):
    def __init__(self, conf):
        pass


class _TestType(metaclass=_Container):
    """Base class for test duration"""

    def __repr__(self):
        return self.__class__.__name__.lower()


class Short(_TestType):
    pass


class Medium(_TestType):
    pass


class Long(_TestType):
    pass


class Check(_TestType):
    includes = [Short, Medium]


class AllTypes(_TestType):
    includes = [Short, Medium, Check, Long]
