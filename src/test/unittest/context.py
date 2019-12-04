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
"""Set of classes that represent the context of single test execution"""

import os
import shlex
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


class ContextBase:
    """Context basic interface and low-level utilities"""
    def __init__(self, build, *args, **kwargs):
        self._elems = []
        self._env = {}
        self.cmd_prefix = ''
        self.build = build

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
                                 'have the attribute "{}"'.format(name))

        # all found elements have the same attribute value
        elif all(e == e for e in elems_with_attr):
            return getattr(elems_with_attr[0], name)

        # more than one element found and they have different attribute values
        else:
            raise AttributeError('Ambiguity while acquiring attribute "{}": '
                                 'implemented by multiple context elements '
                                 'with different values'
                                 .format(name))

    def __str__(self):
        """
        The Context string representation is a concatenation
        (separated by '/') of all its elements string representations.
        """
        s = '{}/'.format(str(self.build))
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
        if hasattr(elem, 'cmd'):
            if self.cmd_prefix:
                raise ValueError('More than one context element '
                                 'defines command line prefix')
            else:
                self.cmd_prefix = elem.cmd

    def add_env(self, env):
        """Add environment variables to those stored by context"""
        futils.add_env_common(self._env, env)

    def setup(self, *args, **kwargs):
        """
        run setup() method for each context element.
        Ignore error if not implemented
        """
        kwargs['tools'] = self.tools
        for e in self._elems:
            try:
                e.setup(*args, **kwargs)
            except AttributeError:
                pass

    def check(self, *args, **kwargs):
        """
        run check() method for each context element.
        Ignore error if not implemented
        """
        kwargs['tools'] = self.tools
        for e in self._elems:
            try:
                e.check(*args, **kwargs)
            except AttributeError:
                pass

    def clean(self, *args, **kwargs):
        """
        run clean() method for each context element.
        Ignore error if not implemented
        """
        kwargs['tools'] = self.tools
        for e in self._elems:
            try:
                e.clean(*args, **kwargs)
            except AttributeError:
                pass

    @property
    def env(self):
        return self._env

    @property
    def tools(self):
        return Tools(self.env, self.build)

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
        proc = self.tools.pmemdetect('-s', path)
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

        tmp = self._env.copy()
        futils.add_env_common(tmp, os.environ.copy())

        # change cmd into list for supbrocess type compliance
        cmd = [cmd, ]

        if sys.platform == 'win32':
            cmd[0] = os.path.join(self.build.exedir, cmd[0]) + '.exe'
        else:
            cmd[0] = os.path.join(self.cwd, cmd[0]) + \
                self.build.exesuffix

            if self.valgrind:
                cmd = self.valgrind.cmd + cmd

        cmd = cmd + list(args)

        if self.conf.tracer:
            cmd = shlex.split(self.conf.tracer) + cmd

            # process stdout and stderr are not redirected - this lets running
            # tracer command in an interactive session
            proc = sp.run(cmd, env=tmp, cwd=self.cwd)
        else:
            proc = sp.run(cmd, env=tmp, cwd=self.cwd,
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


class CtxType(type):
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


def filter_contexts(config_ctx, test_ctx):
    """
    Return contexts that should be used in execution based on
    contexts provided by config and test case
    """
    if not test_ctx:
        return [c for c in config_ctx if not c.explicit]
    return [c for c in config_ctx if c in test_ctx]


def str_to_ctx_common(val, ctx_base_type):
    def class_from_string(name, base):
        if name == 'all':
            return base.__subclasses__()

        try:
            return next(b for b in base.__subclasses__()
                        if str(b) == name.lower())
        except StopIteration:
            print('Invalid context value: "{}".'.format(name))
            raise

    if isinstance(val, list):
        classes = [class_from_string(cl, ctx_base_type) for cl in val]
        return expand(*classes)
    else:
        return expand(class_from_string(val, ctx_base_type))


class _Requirements:
    """
    The class used for storing requirements for the test case. Should be
    referred to through 'add_requirement()' and 'get_requirement()'
    rather than directly.
    """
    pass


def add_requirement(tc, attr, value, **kwargs):
    """Add requirement to the test"""
    if not hasattr(tc, '_requirements'):
        # initialize new requirements storage class if not present
        tc._requirements = _Requirements()

    setattr(tc._requirements, attr, value)
    setattr(tc._requirements, '{}_kwargs'.format(attr), kwargs)


def get_requirement(tc, attr, default):
    """
    Get test requirement set to attribute 'attr', return default
    if not found
    """
    ret_val = default
    ret_kwargs = {}
    try:
        ret_val = getattr(tc._requirements, attr)
        ret_kwargs = getattr(tc._requirements, '{}_kwargs'.format(attr))
    except AttributeError:
        pass

    return ret_val, ret_kwargs


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
