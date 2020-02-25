#
# Copyright 2019-2020, Intel Corporation
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
import shlex
import sys
import itertools
import shutil
import subprocess as sp

import futils
from poolset import _Poolset
import tools
from utils import KiB, MiB, GiB, TiB

try:
    import testconfig
except ImportError:
    sys.exit('Please add valid testconfig.py file - see testconfig.py.example')
config = testconfig.config

try:
    import envconfig
    envconfig = envconfig.config
except ImportError:
    # if file doesn't exist create dummy object
    envconfig = {'GLOBAL_LIB_PATH': ''}


def expand(*classes):
    """Return flatten list of container classes with removed duplications"""
    return list(set(itertools.chain(*classes)))


class ContextBase:
    """Low level context utils."""
    def __init__(self, test, conf, **kwargs):
        self.env = {}
        for ctx in [kwargs['fs'], kwargs['build']]:
            if hasattr(ctx, 'env'):
                self.env.update(ctx.env)
        self.test = test
        self.conf = conf
        self.build = kwargs['build']
        self.fs = kwargs['fs']
        self.valgrind = kwargs['valgrind']
        self.msg = futils.Message(conf)

    def dump_n_lines(self, file, n=None):
        """
        Prints last n lines of given log file. Number of lines printed can be
        modified locally by "n" argument or globally by "dump_lines" in
        testconfig.py file. If none of them are provided, default value is 30.
        """
        if n is None:
            n = config.get('dump_lines', 30)

        file_size = self.get_size(file.name)
        # if file is small enough, read it whole and find last n lines
        if file_size < 100 * MiB:
            lines = list(file)
            length = len(lines)
            if n > length:
                n = length
            lines = lines[-n:]
            lines.insert(0, 'Last {} lines of {} below (whole file has {} lines):{}'
                            ''.format(n, file.name, length, os.linesep))
            for line in lines:
                print(line, end='')
        else:
            # if file is really big, read the last 10KiB and forget about lines
            with open(file.name, 'br') as byte_file:
                byte_file.seek(file_size - 10 * KiB)
                print(byte_file.read().decode('iso_8859_1'))

    def is_devdax(self, path):
        """Checks if given path points to device dax"""
        proc = tools.pmemdetect(self, '-d', path)
        if proc.returncode == tools.PMEMDETECT_ERROR:
            futils.fail(proc.stdout)
        if proc.returncode == tools.PMEMDETECT_TRUE:
            return True
        if proc.returncode == tools.PMEMDETECT_FALSE:
            return False
        futils.fail('Unknown value {} returned by pmemdetect'.format(proc.returncode))

    def supports_map_sync(self, path):
        """Checks if MAP_SYNC is supported on a filesystem from given path"""
        proc = tools.pmemdetect(self, '-s', path)
        if proc.returncode == tools.PMEMDETECT_ERROR:
            futils.fail(proc.stdout)
        if proc.returncode == tools.PMEMDETECT_TRUE:
            return True
        if proc.returncode == tools.PMEMDETECT_FALSE:
            return False
        futils.fail('Unknown value {} returned by pmemdetect'.format(proc.returncode))

    def get_size(self, path):
        """
        Returns size of the file or dax device.
        Value "2**64 - 1" is checked because pmemdetect in case of error prints it.
        """
        proc = tools.pmemdetect(self, '-z', path)
        if int(proc.stdout) != 2**64 - 1:
            return int(proc.stdout)
        futils.fail('Could not get size of the file, it is inaccessible or does not exist')

    def get_free_space(self, dir="."):
        """Returns free space for current file system"""
        _, _, free = shutil.disk_usage(dir)
        return free


class Context(ContextBase):
    """Manage test execution based on values from context classes"""

    def __init__(self, test, conf, **kwargs):
        ContextBase.__init__(self, test, conf, **kwargs)

    @property
    def testdir(self):
        """Test directory on selected filesystem"""
        # Testdir uses 'fs.dir' field  - it is illegal to access it in case of
        # 'Non' fs. Hence it is implemented as a property.
        return os.path.join(self.fs.dir, self.test.testdir)

    def create_holey_file(self, size, path, mode=None):
        """Create a new file with the selected size and name"""
        filepath = os.path.join(self.testdir, path)
        with open(filepath, 'w') as f:
            f.seek(size - 1)
            f.write('\0')
        if mode is not None:
            os.chmod(filepath, mode)
        return filepath

    def create_non_zero_file(self, size, path, mode=None):
        """Create a new non-zeroed file with the selected size and name"""
        filepath = os.path.join(self.testdir, path)
        with open(filepath, 'w') as f:
            f.write('\132' * size)
        if mode is not None:
            os.chmod(filepath, mode)
        return filepath

    def create_zeroed_hdr_file(self, size, path, mode=None):
        """
        Create a new non-zeroed file with a zeroed header and the selected
        size and name
        """
        filepath = os.path.join(self.testdir, path)
        with open(filepath, 'w') as f:
            f.write('\0' * HEADER_SIZE)
            f.write('\132' * (size - HEADER_SIZE))
        if mode is not None:
            os.chmod(filepath, mode)
        return filepath

    def require_free_space(self, space):
        if self.get_free_space(self.testdir) < space:
            futils.skip('Not enough free space ({} MiB required)'
                        .format(space / MiB))

    def mkdirs(self, path, mode=None):
        """
        Creates directory along with all parent directories required. In the
        case given path already exists do nothing.
        """
        dirpath = os.path.join(self.testdir, path)
        if mode is None:
            os.makedirs(dirpath, exist_ok=True)
        else:
            os.makedirs(dirpath, mode, exist_ok=True)

    def new_poolset(self, path):
        return _Poolset(path, self)

    def exec(self, cmd, *args, expected_exit=0):
        """Execute binary in current test context"""

        env = {**self.env, **os.environ.copy(), **self.test.utenv}

        # change cmd into list for supbrocess type compliance
        cmd = [cmd, ]

        if sys.platform == 'win32':
            env['PATH'] = self.build.libdir + os.pathsep +\
                envconfig['GLOBAL_LIB_PATH'] + os.pathsep +\
                env.get('PATH', '')
            cmd[0] = os.path.join(self.build.exedir, cmd[0]) + '.exe'

        else:
            if self.test.ld_preload:
                env['LD_PRELOAD'] = env.get('LD_PRELOAD', '') + os.pathsep +\
                    self.test.ld_preload
                self.valgrind.handle_ld_preload(self.test.ld_preload)
            env['LD_LIBRARY_PATH'] = self.build.libdir + os.pathsep +\
                envconfig['GLOBAL_LIB_PATH'] + os.pathsep +\
                env.get('LD_LIBRARY_PATH', '')
            cmd[0] = os.path.join(self.test.cwd, cmd[0]) + self.build.exesuffix

            if self.valgrind:
                cmd = self.valgrind.cmd + cmd

        cmd = cmd + list(args)

        if self.conf.tracer:
            cmd = shlex.split(self.conf.tracer) + cmd

            # process stdout and stderr are not redirected - this lets running
            # tracer command in interactive session
            proc = sp.run(cmd, env=env, cwd=self.test.cwd)
        else:
            proc = sp.run(cmd, env=env, cwd=self.test.cwd,
                          timeout=self.conf.timeout, stdout=sp.PIPE,
                          stderr=sp.STDOUT, universal_newlines=True)

        if proc.returncode != expected_exit:
            futils.fail(proc.stdout, exit_code=proc.returncode)

        if sys.platform != 'win32' and expected_exit == 0 \
                and not self.valgrind.validate_log():
            futils.fail(proc.stdout)

        self.msg.print_verbose(proc.stdout)


class _CtxType(type):
    """Metaclass for context classes that can stand for multiple classes"""
    def __init__(cls, name, bases, dct):
        type.__init__(cls, name, bases, dct)

        # it is supposed that context class has a user defined base class
        if cls.__base__.__name__ == 'object':
            return

        # context class preferred to be used when 'Any' test option is provided
        if not hasattr(cls, 'preferred'):
            cls.is_preferred = False

        # if explicit, run only if the test case explicitly specifies this
        # context in its own configuration
        if not hasattr(cls, 'explicit'):
            cls.explicit = False

        if hasattr(cls, 'includes'):
            cls.includes = expand(*cls.includes)
        else:
            # class that does not include other classes, includes itself
            cls.includes = [cls]

        # set instance (not class) method
        setattr(cls, '__repr__', lambda cls: cls.__class__.__name__.lower())

    def __repr__(cls):
        return cls.__name__.lower()

    def __str__(cls):
        return cls.__name__.lower()

    def __iter__(cls):
        for c in cls.includes:
            yield c

    def factory(cls, conf, *classes):
        return [c(conf) for c in expand(*classes)]


class _Build(metaclass=_CtxType):
    """Base and factory class for standard build classes"""
    exesuffix = ''


class Debug(_Build):
    """Set the context for debug build"""
    is_preferred = True

    def __init__(self, conf):
        if sys.platform == 'win32':
            self.exedir = futils.WIN_DEBUG_EXEDIR
        self.libdir = futils.DEBUG_LIBDIR


class Release(_Build):
    """Set the context for release build"""
    is_preferred = True

    def __init__(self, conf):
        if sys.platform == 'win32':
            self.exedir = futils.WIN_RELEASE_EXEDIR
        self.libdir = futils.RELEASE_LIBDIR


# Build types not available on Windows
if sys.platform != 'win32':
    class Static_Debug(_Build):
        """Sets the context for static_debug build"""

        def __init__(self, conf):
            self.exesuffix = '.static-debug'
            self.libdir = futils.DEBUG_LIBDIR

    class Static_Release(_Build):
        """Sets the context for static_release build"""

        def __init__(self, conf):
            self.exesuffix = '.static-nondebug'
            self.libdir = futils.RELEASE_LIBDIR


class _Fs(metaclass=_CtxType):
    """Base class for filesystem classes"""


class Pmem(_Fs):
    """Set the context for pmem filesystem"""
    is_preferred = True

    def __init__(self, conf):
        self.dir = os.path.abspath(conf.pmem_fs_dir)
        if conf.fs_dir_force_pmem == 1:
            self.env = {'PMEM_IS_PMEM_FORCE': '1'}


class Nonpmem(_Fs):
    """Set the context for nonpmem filesystem"""

    def __init__(self, conf):
        self.dir = os.path.abspath(conf.non_pmem_fs_dir)


class Non(_Fs):
    """
    No filesystem is used. Accessing some fields of this class is prohibited.
    """
    explicit = True

    def __init__(self, conf):
        pass

    def __getattribute__(self, name):
        if name in ('dir',):
            raise AttributeError("fs '{}' attribute cannot be used for '{}' fs"
                                 .format(name, self))
        else:
            return super().__getattribute__(name)


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
