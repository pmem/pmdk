# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#

"""
Set of classes that represent the context of single test execution
as well as functions used for setting test requirements and adding
test parameters.

The test context is visible to the RUNTESTS main script
as well as the test user as a Context class.
The context class is constructed with use of 'elements' classes,
that are affiliated with test requirements and test parameters.
"""

import os
import shlex
import sys
import itertools
import shutil
import subprocess as sp
import collections
import contextlib
from inspect import ismethod

import configurator
import futils
from poolset import _Poolset
from tools import Tools
from consts import KiB, MiB, HEADER_SIZE
from unsafe_shutdown import UnsafeShutdown
from badblock import BadBlock

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
    envconfig = {'GLOBAL_LIB_PATH': '', 'PMEM2_AVX512F_ENABLED': ''}


def expand(*classes):
    """Return flatten list of container classes with removed duplications"""
    return list(set(itertools.chain(*classes)))


class ContextBase:
    """Context basic interface and low-level utilities

    Main context class responsibility is to manage context elements
    appended to it (such as Valgrind, filesystem type, etc.),
    which provide concrete contextualized behavior implementation.

    Attributes:
        _elems (list): list of context elements, such as Valgrind tool,
            filesystem type, etc.
        cmd_prefix (str): test execution command prefix - utilized by
            additional test execution tools, such as Valgrind.
            Defaults to an empty string.
        build (Build): test build

    """

    def __init__(self, build, **elements):
        """
        Args:
            build (Build): test execution build type
            **elements: context elements stored as ContextBase attributes
                named by keyword argument key.
                The element is therefore accessible from ContextBase class.
                Apart from that, below element class methods and attributes
                are handled (if present) by ContextBase class:

                env (dict): environment variables set by the element
                cmd (string): set to cmd_prefix
                setup, check, clean methods: called by corresponding
                    ContextBase methods which are in turn called in
                    Basetest._execute()

        """
        self._elems = []
        self._env = {}
        self.cmd_prefix = ''
        self.build = build

        # for keyword arguments, each kwarg value is set as a
        # context attribute with its key used as an attribute name.
        for k, v in elements.items():
            self.add_ctx_elem(v)
            setattr(self, str(k), v)

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

        # if element has a 'name' attribute, it is accessible from Context
        # through it, otherwise its attribute name is generated
        # from its string representation
        if hasattr(elem, 'name'):
            setattr(self, elem.name, elem)
        else:
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
            with contextlib.suppress(AttributeError):
                e.setup(*args, **kwargs)

    def check(self, *args, **kwargs):
        """
        run check() method for each context element.
        Ignore error if not implemented
        """
        kwargs['tools'] = self.tools
        for e in self._elems:
            with contextlib.suppress(AttributeError):
                e.check(*args, **kwargs)

    def clean(self, *args, **kwargs):
        """
        run clean() method for each context element.
        Ignore error if not implemented
        """
        kwargs['tools'] = self.tools
        for e in self._elems:
            with contextlib.suppress(AttributeError):
                e.clean(*args, **kwargs)

    @property
    def env(self):
        """Get context environment variables"""
        return self._env

    @property
    def tools(self):
        """Get test tools"""
        return Tools(self.env, self.build)

    @property
    def usc(self):
        return UnsafeShutdown()

    @property
    def badblock(self):
        return BadBlock(self.tools)

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

    def get_free_space(self, dir="."):
        """Returns free space for current file system"""
        _, _, free = shutil.disk_usage(dir)
        return free


class Context(ContextBase):
    """Manage test execution based on values from context classes"""

    def __init__(self, *args, **kwargs):
        self.conf = configurator.Configurator().config
        self.msg = futils.Message(self.conf.unittest_log_level)
        ContextBase.__init__(self, *args, **kwargs)

    def new_poolset(self, path):
        return _Poolset(path, self)

    def exec(self, cmd, *args, expected_exitcode=0, stderr_file=None,
             stdout_file=None):
        """
        Execute binary in the current test context as a separate process.

        Execution takes place in test cwd and uses environment variables
        stored in Context 'env' attribute. Timeout for the execution
        is set based on the execution configuration.

        Args:
            cmd (str): command to be executed
            *args: Variable length command arguments list
            expected_exitcode (int): if process exit code differs from
                expected, Fail is thrown. Defaults to 0. Ignored
                if set to None.
            stderr_file (str): path to file in which stderr output is
                stored. Stored in a string if None. Defaults to None.
            stdout_file (str): path to file in which stdout output is
                stored. Stored in a string if None. Defaults to None.

            If neither stderr_file nor stdout_file are set, both outputs
            are merged into single stdout output and stored in a string.
        """

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

        # cast all provided args to strings (required by subprocess run())
        # so that exec() can accept args of any printable type
        cmd.extend([str(a) for a in args])

        if self.conf.tracer:
            cmd = shlex.split(self.conf.tracer) + cmd

            # process stdout and stderr are not redirected - this lets running
            # tracer command in an interactive session
            proc = sp.run(cmd, env=tmp, cwd=self.cwd)
        else:
            if stderr_file:
                f = open(os.path.join(self.cwd, stderr_file), 'w')

            # let's create a dictionary of arguments to the run func
            run_kwargs = {
                'env': tmp,
                'cwd': self.cwd,
                'timeout': self.conf.timeout,
                'stdout': sp.PIPE,
                'universal_newlines': True,
                'stderr': sp.STDOUT if stderr_file is None else f}

            proc = sp.run(cmd, **run_kwargs)

            if stderr_file:
                f.close()

        if expected_exitcode is not None and \
           proc.returncode != expected_exitcode:
            futils.fail(proc.stdout, exit_code=proc.returncode)

        if stdout_file is not None:
            with open(os.path.join(self.cwd, stdout_file), 'w') as f:
                f.write(proc.stdout)

        self.msg.print_verbose(proc.stdout)

    def create_holey_file(self, size, path, mode=None):
        """
        Create a new file in test testdir
        with the selected size and name.
        """
        filepath = os.path.join(self.testdir, path)
        with open(filepath, 'w') as f:
            if size > 0:
                f.seek(size - 1)
                f.write('\0')
        if mode is not None:
            os.chmod(filepath, mode)
        return filepath

    def create_non_zero_file(self, size, path, mode=None):
        """
        Create a new non-zeroed file in test testdir
        with the selected size and name.
        """
        filepath = os.path.join(self.testdir, path)
        with open(filepath, 'w') as f:
            f.write('\132' * size)
        if mode is not None:
            os.chmod(filepath, mode)
        return filepath

    def create_zeroed_hdr_file(self, size, path, mode=None):
        """
        Create a new non-zeroed file with a zeroed header and the selected
        size and name.
        """
        filepath = os.path.join(self.testdir, path)
        with open(filepath, 'w') as f:
            f.write('\0' * HEADER_SIZE)
            f.write('\132' * (size - HEADER_SIZE))
        if mode is not None:
            os.chmod(filepath, mode)
        return filepath

    def require_free_space(self, space):
        """
        Skip test if not enough free space is available in test testdir.
        """
        if self.get_free_space(self.testdir) < space:
            futils.skip('Not enough free space ({} MiB required)'
                        .format(space / MiB))

    def mkdirs(self, path, mode=None):
        """
        Creates directory in test testdir along with all parent
        directories required. In the case given path already exists
        do nothing.
        """
        dirpath = os.path.join(self.testdir, path)
        if mode is None:
            os.makedirs(dirpath, exist_ok=True)
        else:
            os.makedirs(dirpath, mode, exist_ok=True)


class CtxType(type):
    """
    Metaclass for context classes that can stand for multiple classes

    Attributes:
        is_preferred (bool): if True, context class is preferred
            to be chosen in case of Any execution setting.
        explicit (bool): if True, the context class is executed
            only if explicitly requested by test implementation,
            i. e. it is ignored if specified only in external
            configuration.
        includes: list of context classes included by this class,
            e. g. Short and Medium test types are included by
            Check test type

    """
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
    contexts provided by config and test case.
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


def _req_prefix(attr):
    """
    Add prefix to requirement attribute name. Used to mitigate the risk
    of name clashes.
    """
    return '_require{}'.format(attr)


def add_requirement(tc, attr, value, **kwargs):
    """
    Add requirement to the test

    Requirements are added to the test as attributes dynamically
    appended to the test class. Their names are
    additionally mangled with _req_prefix() function
    to avoid accidental overwriting of any existing
    test class attributes as well as unnecessary access by the
    test user - these attributes are meant to be used directly
    only by requirements handling functions.

    Args:
        tc (BaseTest): test case to which the requirement
            is added
        attr (str): attribute name of the requirement.
            additionally mangled with _req_prefix() function.
        value: requirement value
        **kwargs: additional keyword arguments that may be used
            by requirement implementation
    """
    setattr(tc, _req_prefix(attr), value)
    setattr(tc, _req_prefix('{}_kwargs'.format(attr)), kwargs)


def get_requirement(tc, attr, default):
    """
    Get test requirement added with add_requirement() function
    with specific attribute name.

    Args:
        tc (BaseTest): test case class into which requirements were
            added
        attr (str): requirement attribute name - as previously added by
            add_requirement() function
        default: default requirement value if not found

    Returns:
        requirement value, requirement keyword arguments
            (as provided by 'kwargs' argument to add_requirement() function)
    """
    ret_val = default
    ret_kwargs = {}
    try:
        ret_val = getattr(tc, _req_prefix(attr))
        ret_kwargs = getattr(tc, _req_prefix('{}_kwargs'.format(attr)))
    except AttributeError:
        pass

    return ret_val, ret_kwargs


class TestParam:
    """
    TestParam manages single test parameter provided to test with add_params()
    decorator.
    Test parameters are added to context class as its elements - therefore the
    'value' object may implement interface handled by ContextBase class, such
    as setup() and clean()
    """
    def __init__(self, name, value):
        self.name = name
        self.value = value

    def __str__(self):
        return '{}: {}'.format(self.name, self.value)

    def __call__(self):
        return self.value

    def setup(self, **kwargs):
        if hasattr(self.value, 'setup') and ismethod(self.value.setup):
            self.value.setup(**kwargs)

    def check(self, **kwargs):
        if hasattr(self.value, 'check') and ismethod(self.value.check):
            self.value.check(**kwargs)

    def clean(self, **kwargs):
        if hasattr(self.value, 'clean') and ismethod(self.value.clean):
            self.value.clean(**kwargs)


PARAMS_ATTR = '_params_'


def add_params(name, values):
    """
    Add parameters to the test case.

    Parameters will be accessible from test through context 'name' attribute.
    The value of single parameter can be accessed by calling this attribute,
    e.g. for:
    add_params('param', params_list)
    single parameter provided within 'params_list' is accessible
    by calling ctx.param().
    """
    params = [TestParam(name, v) for v in values]

    def wrapped(tc):
        if hasattr(tc, PARAMS_ATTR):
            getattr(tc, PARAMS_ATTR).update({name: params})
        else:
            setattr(tc, PARAMS_ATTR, {name: params})
        return tc

    return wrapped


def get_params(tc):
    """Get parameters added to test case"""
    try:
        return getattr(tc, PARAMS_ATTR)
    except AttributeError:
        return {}


class Any:
    """
    The test context attribute signifying that specific context value is not
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
        # if no preferred is found, pick the first non-explicit one
        ret = [c for c in conf_ctx if not c.explicit]
        if ret:
            return ret[0]
        else:
            config = configurator.Configurator().config
            msg = futils.Message(config.unittest_log_level)
            msg.print_verbose('No valid "Any" context found')
            return None


class _NoContext(collections.UserList):
    """
    May be safely returned by context element class 'filter' method if
    its items are not required during test execution
    (e. g. no dax devices required by test)
    """

    def __init__(self):
        self.data = []
        self.data.append(False)

    def __bool__(self):
        return False


NO_CONTEXT = _NoContext()
