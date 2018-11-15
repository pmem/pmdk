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
import subprocess as sp

import helpers as hlp


def expand(*classes):
    """Return flatten list of container classes with removed duplications"""
    return list(set(itertools.chain(*classes)))


class Context:
    """Manage test execution based on values from context classes"""

    def __init__(self, test, conf, fs, build):
        self.test = test
        self.conf = conf
        self.build = build
        self.msg = hlp.Message(conf)
        self.fs = fs
        self.testdir = os.path.join(fs.dir, test.testdir)

    def create_holey_file(self, size, name):
        """Create a new file with the selected size and name"""
        filepath = os.path.join(self.testdir, name)
        with open(filepath, 'w') as f:
            f.seek(size)
            f.write('\0')
        return filepath

    def exec(self, cmd, args=''):
        """Execute test C binary in current test context"""
        env = {**os.environ.copy(), **self.test.utenv}
        if sys.platform == 'win32':
            env['PATH'] = env['PATH'] + os.pathsep + self.build.libdir
            cmd = os.path.join(self.build.exedir, cmd) + '.exe'
        else:
            env['LD_LIBRARY_PATH'] = os.pathsep + self.build.libdir
            cmd = os.path.join(self.test.cwd, cmd) + self.build.exesuffix

        try:
            proc = sp.run([cmd, args], env=env, cwd=self.test.cwd,
                          timeout=self.conf.test_timeout, stdout=sp.PIPE,
                          stderr=sp.STDOUT, universal_newlines=True)
        except sp.TimeoutExpired:
            self.test.timeout = True
            self.msg.print('Skipping: {} {} timed out{}'.format(
                self.test.name, hlp.Color.RED, hlp.Color.END))

        if proc.returncode != 0:
            self.test.failed = True
            print(proc.stdout)


class _CtxType(type):
    """Metaclass for context classes that can stand for multiple classes"""
    def __init__(cls, name, bases, dct):
        type.__init__(cls, name, bases, dct)

        # It is supposed that context class has a user defined base class
        if cls.__base__.__name__ == 'object':
            return

        if hasattr(cls, 'includes'):
            cls.includes = expand(*cls.includes)
        else:
            # class that does not include other classes, includes itself
            cls.includes = [cls]

        # set instance (not class) method
        setattr(cls, '__repr__', lambda cls: cls.__class__.__name__.lower())

    def __repr__(cls):
        return cls.__name__.lower()

    def __iter__(cls):
        for c in cls.includes:
            yield c

    def should_run(cls, conf_types):
        return bool([c for c in cls if c in conf_types])

    def factory(cls, conf, *classes):
        return [c(conf) for c in expand(*classes)]


class _Build(metaclass=_CtxType):
    """
    Base and factory class for standard build classes.
    """
    exesuffix = ''


class Debug(_Build):
    """This class sets the context for debug build"""

    def __init__(self, conf):
        if sys.platform == 'win32':
            self.exedir = hlp.WIN_DEBUG_EXEDIR
        self.libdir = hlp.DEBUG_LIBDIR


class Nondebug(_Build):
    """This class sets the context for nondebug build"""

    def __init__(self, conf):
        if sys.platform == 'win32':
            self.exedir = hlp.WIN_NONDEBUG_EXEDIR
        self.libdir = hlp.NONDEBUG_LIBDIR


# Build types not available on Windows
if sys.platform != 'win32':
    class Static_Debug(_Build):
        """Sets the context for static_debug build"""

        def __init__(self, conf):
            self.exesuffix = '.static-debug'
            self.libdir = hlp.DEBUG_LIBDIR

    class Static_Nondebug(_Build):
        """Sets the context for static_nondebug build"""

        def __init__(self, conf):
            self.exesuffix = '.static-nondebug'
            self.libdir = hlp.NONDEBUG_LIBDIR


class AllBuilds(_Build):
    if sys.platform == 'win32':
        includes = [Debug, Nondebug]
    else:
        includes = [Debug, Nondebug, Static_Debug, Static_Nondebug]


class _Fs(metaclass=_CtxType):
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


class _TestType(metaclass=_CtxType):
    """Base class for test duration"""


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
