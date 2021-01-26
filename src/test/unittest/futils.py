# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2021, Intel Corporation

"""Test framework utilities"""

from os.path import join, abspath, dirname
import os
import sys

import configurator

# Constant paths to repository elements
ROOTDIR = abspath(join(dirname(__file__), '..'))

WIN_DEBUG_BUILDDIR = abspath(join(ROOTDIR, '..', 'x64', 'Debug'))
WIN_DEBUG_EXEDIR = abspath(join(WIN_DEBUG_BUILDDIR, 'tests'))

WIN_RELEASE_BUILDDIR = abspath(join(ROOTDIR, '..', 'x64', 'Release'))
WIN_RELEASE_EXEDIR = abspath(join(WIN_RELEASE_BUILDDIR, 'tests'))

if sys.platform == 'win32':
    DEBUG_LIBDIR = abspath(join(WIN_DEBUG_BUILDDIR, 'libs'))
    RELEASE_LIBDIR = abspath(join(WIN_RELEASE_BUILDDIR, 'libs'))
else:
    DEBUG_LIBDIR = abspath(join(ROOTDIR, '..', 'debug'))
    RELEASE_LIBDIR = abspath(join(ROOTDIR, '..', 'nondebug'))


def get_tool_path(ctx, name):
    if sys.platform == 'win32':
        if str(ctx.build) == 'debug':
            return abspath(join(WIN_DEBUG_BUILDDIR, 'libs', name))
        else:
            return abspath(join(WIN_RELEASE_BUILDDIR, 'libs', name))
    else:
        return abspath(join(ROOTDIR, '..', 'tools', name, name))


def get_test_tool_path(build, name):
    if sys.platform == 'win32':
        if str(build) == 'debug':
            return abspath(join(WIN_DEBUG_BUILDDIR, 'tests', name))
        else:
            return abspath(join(WIN_RELEASE_BUILDDIR, 'tests', name))
    else:
        return abspath(join(ROOTDIR, 'tools', name, name))


def get_lib_dir(ctx):
    if str(ctx.build) == 'debug':
        return DEBUG_LIBDIR
    else:
        return RELEASE_LIBDIR


def get_example_path(ctx, libname, name):
    """
    Get the path to the example binary.
    Paths to examples differ on Windows and Unix systems. On Windows,
    the example binaries have a specific name: ex_libname_name.
    On Unix systems, the example binaries are located in the catalog
    "lib + libname/name" and have the same name as .c file.
    """
    if sys.platform == 'win32':
        binname = '_'.join(['ex', libname, name])
        if str(ctx.build) == 'debug':
            return abspath(join(WIN_DEBUG_BUILDDIR, 'examples', binname))
        else:
            return abspath(join(WIN_RELEASE_BUILDDIR, 'examples', binname))
    else:
        return abspath(join(ROOTDIR, '..', 'examples', 'lib' + libname,
                            name, name))


def tail(file, n):
    """
    Replace the file content with the n last lines from the existing file.
    The original file is saved under the name with ".old" suffix.
    """
    with open(file, 'r') as f:
        lines = f.readlines()
        last_lines = lines[-n:]
    os.rename(file, file + ".old")
    with open(file, 'w') as f:
        for line in last_lines:
            f.write(line)


def count(file, substring):
    """
    Count the number of occurrences of a string in the given file.
    """
    with open(file, 'r') as f:
        content = f.read()

    return content.count(substring)


class Color:
    """
    Set the font color. This functionality relies on ANSI escape sequences
    and is currently disabled for Windows.
    """
    if sys.platform != 'win32':
        RED = '\33[91m'
        GREEN = '\33[92m'
        END = '\33[0m'
    else:
        RED, GREEN, END = '', '', ''


class Message:
    """Simple level based logger"""

    def __init__(self, level):
        self.level = level

    def print(self, msg):
        if self.level >= 1:
            print(msg)

    def print_verbose(self, msg):
        if self.level >= 2:
            print(msg)


class Fail(Exception):
    """Thrown when test fails"""

    def __init__(self, msg):
        super().__init__(msg)
        self.messages = []
        self.messages.append(msg)

    def __str__(self):
        ret = '\n'.join(self.messages)
        return ret


def fail(msg, exit_code=None):
    if exit_code is not None:
        msg = '{}{}Error {}'.format(msg, os.linesep, exit_code)
    raise Fail(msg)


class Skip(Exception):
    """Thrown when test should be skipped"""

    def __init__(self, msg):
        super().__init__(msg)
        config = configurator.Configurator().config
        if config.fail_on_skip:
            raise Fail(msg)

        self.messages = []
        self.messages.append(msg)

    def __str__(self):
        ret = '\n'.join(self.messages)
        return ret


def skip(msg):
    raise Skip(msg)


def set_kwargs_attrs(cls, kwargs):
    for k, v in kwargs.items():
        setattr(cls, '{}'.format(k), v)


def add_env_common(src, added):
    """
    A common implementation of adding an environment variable
    to the 'src' environment variables dictionary - taking into account
    that the variable may or may be not already defined.
    """
    for k, v in added.items():
        if k in src:
            src[k] = v + os.pathsep + src[k]
        else:
            src.update({k: v})


def to_list(var, *types):
    """
    Some variables may be provided by the user either as a single instance of
    a type or a sequence of instances (e. g. a string or list of strings).
    To be conveniently treated by the framework code, their types
    should be unified - casted to lists.
    """
    if isinstance(var, tuple(types)):
        return [var, ]
    else:
        return var
