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
"""Set of classes that represent the context of single test execution
(like build, filesystem, test type)"""

import os
import sys
import itertools
import shutil
import subprocess as sp

import configurator
import futils
from poolset import _Poolset
from tools import Tools
from utils import KiB, MiB, HEADER_SIZE

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


def add_env_common(src, added):
    """
    Common implementation of adding environment variable to the 'src'
    environment variables dictionary - taking into account that the variable
    may or may be not already defined.
    """
    for k, v in added.items():
        if k in src:
            src[k] = src[k] + os.pathsep + v
        else:
            src.update({k: v})


class ContextBase:
    """Context basic interface and low level utilities"""
    def __init__(self, *args, **kwargs):
        self._elems = []
        self._env = {}

        # for positional arguments, each arg is added to the anonymous
        # context elements list
        for arg in args:
            self.add_ctx_elem(arg)

        # for keyword arguments, each kwarg value is additionally set as a
        # context attribute with its key assigned as an attribute name.
        for k, v in kwargs.items():
            self.add_ctx_elem(v)
            setattr(self, '{}'.format(k), v)

    def __getattr__(self, name):
        """
        If context class itself does not have an acquired attribute
        check if one of its elements has it and return it.
        """
        elems_with_attr = [e for e in self._elems if hasattr(e, name)]

        # no context elements with requested attribute found
        if not elems_with_attr:
            raise AttributeError('Neither context nor any of its elements '
                                 'has an attribute "{}"'.format(name))

        # exactly one element with attribute found
        # or all found elements have the same attribute value
        elif len(elems_with_attr) == 1 or all(e == e for e in elems_with_attr):
            return getattr(elems_with_attr[0], name)

        # more than one element found and they have different attribute values
        else:
            raise AttributeError('Ambiguity while acquiring attribute "{}": '
                                 'more than one context element implements it'
                                 .format(name))

    def __str__(self):
        """
        Context string representation is a concatenation (separated by '/') of
        all its elements string representations.
        """
        s = ''
        for e in self._elems:
            if e:
                s = s + str(e) + '/'
        s = s[:-1]
        return s

    def add_ctx_elem(self, elem):
        """
        Add element to the context. Update context environment variables
        with element's environment variables (if present)
        """
        self._elems.append(elem)
        setattr(self, str(elem), elem)

        if hasattr(elem, 'env'):
            self.add_env(elem.env)
        if hasattr(elem, 'cmd_prefix'):
            self.add_cmd_prefix(elem)

    def add_env(self, env):
        """Add environment variables to those stored by context"""
        add_env_common(self._env, env)

    def setup(self, *args, **kwargs):
        """
        run setup() method for each context element. Ignore error if not
        implemented
        """
        for e in self._elems:
            kw = kwargs.copy()
            try:
                if isinstance(e, _Granularity):
                    try:
                        kw['tools'] = self.tools
                    except AttributeError:
                        futils.Skip('granularity needs tools for setup')

                e.setup(*args, **kw)
            except AttributeError:
                pass

    def check(self, *args, **kwargs):
        """
        run check() method for each context element. Ignore error if not
        implemented
        """
        for e in self._elems:
            try:
                e.check(*args, **kwargs)
            except AttributeError:
                pass

    def clean(self, *args, **kwargs):
        """
        run clean() method for each context element. Ignore error if not
        implemented
        """
        for e in self._elems:
            try:
                e.clean(*args, **kwargs)
            except AttributeError:
                pass

    @property
    def tools(self):
        if hasattr(self, 'build'):
            return Tools(self.env, self.build)
        else:
            futils.Skip('could not use tools without build')

    @property
    def env(self):
        return self._env

    def dump_n_lines(self, file, n=-1):
        """
        Prints last n lines of given log file. Number of lines printed can be
        modified locally by "n" argument or globally by "dump_lines" in
        testconfig.py file. If none of them are provided, default value is 30.
        """
        if n < 0:
            n = config.get('dump_lines', 30)

        file_size = self.get_size(file.name)
        # if file is small enough, read it whole and find last n lines
        if file_size < 100 * MiB:
            lines = list(file)
            length = len(lines)
            if n > length:
                n = length
            lines = lines[-n:]
            lines.insert(0, 'Last {} lines of {} below '
                         '(whole file has {} lines):{}'
                         .format(n, file.name, length, os.linesep))
            for line in lines:
                print(line, end='')
        else:
            # if file is really big, read the last 10KiB and forget about lines
            with open(file.name, 'br') as byte_file:
                byte_file.seek(file_size - 10 * KiB)
                print(byte_file.read().decode('iso_8859_1'))

    def is_devdax(self, path):
        """Checks if given path points to device dax"""
        proc = self.tools.pmemdetect('-d', path)
        if proc.returncode == self.tools.PMEMDETECT_ERROR:
            futils.fail(proc.stdout)
        if proc.returncode == self.tools.PMEMDETECT_TRUE:
            return True
        if proc.returncode == self.tools.PMEMDETECT_FALSE:
            return False
        futils.fail('Unknown value {} returned by pmemdetect'
                    .format(proc.returncode))

    def supports_map_sync(self, path):
        """Checks if MAP_SYNC is supported on a filesystem from given path"""
        proc = self.tools.pmemdetect(self, '-s', path)
        if proc.returncode == self.tools.PMEMDETECT_ERROR:
            futils.fail(proc.stdout)
        if proc.returncode == self.tools.PMEMDETECT_TRUE:
            return True
        if proc.returncode == self.tools.PMEMDETECT_FALSE:
            return False
        futils.fail('Unknown value {} returned by pmemdetect'
                    .format(proc.returncode))

    def get_size(self, path):
        """
        Returns size of the file or dax device.
        Value "2**64 - 1" is checked because pmemdetect
        prints it in case of error.
        """
        proc = self.tools.pmemdetect('-z', path)
        if int(proc.stdout) != 2**64 - 1:
            return int(proc.stdout)
        futils.fail('Could not get size of the file, '
                    'it is inaccessible or does not exist')

    def get_free_space(self):
        """Returns free space for current file system"""
        _, _, free = shutil.disk_usage(".")
        return free


class Context(ContextBase):
    """Manage test execution based on values from context classes"""

    def __init__(self, *args, **kwargs):
        self.conf = configurator.Configurator().config
        self.msg = futils.Message(self.conf.unittest_log_level)
        ContextBase.__init__(self, *args, **kwargs)

    def new_poolset(self, path):
        return _Poolset(path, self)

    def exec(self, cmd, *args, expected_exitcode=0):
        """Execute binary in current test context"""
        cmd_args = ' '.join(args) if args else ''

        tmp = self._env.copy()
        add_env_common(tmp, os.environ.copy())

        if sys.platform == 'win32':
            cmd = os.path.join(self.exedir, cmd) + '.exe'

        else:
            cmd = os.path.join(self.cwd, cmd) + self.exesuffix
            cmd = '{} {}'.format(self.cmd, cmd)

        cmd = '{} {}'.format(cmd, cmd_args)
        if self.conf.tracer:
            cmd = '{} {}'.format(self.conf.tracer, cmd)

            # process stdout and stderr are not redirected - this lets running
            # tracer command in interactive session
            proc = sp.run(cmd, env=tmp, cwd=self.cwd, shell=True)
        else:
            proc = sp.run(cmd, env=tmp, cwd=self.cwd, shell=True,
                          timeout=self.conf.timeout, stdout=sp.PIPE,
                          stderr=sp.STDOUT, universal_newlines=True)

        if expected_exitcode is not None and \
           proc.returncode != expected_exitcode:
            futils.fail(proc.stdout, exit_code=proc.returncode)

        self.msg.print_verbose(proc.stdout)

    def create_holey_file(self, size, path, mode=None):
        """Create a new file with the selected size and name"""
        filepath = os.path.join(self.testdir, path)
        with open(filepath, 'w') as f:
            if size > 0:
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


class _Build(metaclass=_CtxType):
    """Base and factory class for standard build classes"""
    exesuffix = ''

    def set_env_common(self):
        if sys.platform == 'win32':
            self.env = {'PATH': self.libdir}
        else:
            self.env = {'LD_LIBRARY_PATH': self.libdir}


class Debug(_Build):
    """Set the context for debug build"""
    is_preferred = True

    def __init__(self):
        if sys.platform == 'win32':
            self.exedir = futils.WIN_DEBUG_EXEDIR
        self.libdir = futils.DEBUG_LIBDIR
        self.set_env_common()


class Release(_Build):
    """Set the context for release build"""
    build = 'release'
    is_preferred = True

    def __init__(self):
        if sys.platform == 'win32':
            self.exedir = futils.WIN_RELEASE_EXEDIR
        self.libdir = futils.RELEASE_LIBDIR
        self.set_env_common()


# Build types not available on Windows
if sys.platform != 'win32':
    class Static_Debug(_Build):
        """Sets the context for static_debug build"""
        build = 'static_debug'

        def __init__(self):
            self.exesuffix = '.static-debug'
            self.libdir = futils.DEBUG_LIBDIR

    class Static_Release(_Build):
        """Sets the context for static_release build"""
        build = 'static_release'

        def __init__(self):
            self.exesuffix = '.static-nondebug'
            self.libdir = futils.RELEASE_LIBDIR


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


class _Granularity(metaclass=_CtxType):
    gran_detecto_arg = None

    def __init__(self, **kwargs):
        futils.set_kwargs_attrs(self, kwargs)
        self.config = configurator.Configurator().config
        self.force = False

    def setup(self, tools=None):
        if not os.path.exists(self.testdir):
            os.makedirs(self.testdir)

        check_page = tools.gran_detecto(self.testdir, self.gran_detecto_arg)
        if not self.force and check_page.returncode != 0:
            msg = check_page.stdout
            detect = tools.gran_detecto(self.testdir, '-d')
            msg = '{}{}{}'.format(os.linesep, msg, detect.stdout)
            raise futils.Fail('Granularity check for {} failed: {}'
                              .format(self.testdir, msg))

    def clean(self):
        shutil.rmtree(self.testdir, ignore_errors=True)


class Page(_Granularity):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.gran_detecto_arg = '-p'
        self.dir = os.path.abspath(self.config.page_fs_dir)
        self.testdir = os.path.join(self.dir, self.tc_dirname)
        self.force = kwargs['force_page']


class CacheLine(_Granularity):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.gran_detecto_arg = '-c'
        self.dir = os.path.abspath(self.config.cacheline_fs_dir)
        self.testdir = os.path.join(self.dir, self.tc_dirname)
        self.force = kwargs['force_cacheline']


class Byte(_Granularity):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.gran_detecto_arg = '-b'
        self.dir = os.path.abspath(self.config.byte_fs_dir)
        self.testdir = os.path.join(self.dir, self.tc_dirname)
        self.force = kwargs['force_byte']


class Non(_Granularity):
    """
    No filesystem is used. Accessing some fields of this class is prohibited.
    """
    explicit = True

    def __init__(self, **kwargs):
        pass

    def setup(self, tools=None):
        pass

    def cleanup(self):
        pass

    def __getattribute__(self, name):
        if name in ('dir',):
            raise AttributeError("'{}' attribute cannot be used if the test "
                                 "is meant to not use any filesystem"
                                 .format(name, self))
        else:
            return super().__getattribute__(name)


# test case attributes that refer to selected context classes, their
# respective config field names and context base classes
CTX_COMPONENTS = (
    ('build', _Build),
    ('granularity', _Granularity)
)