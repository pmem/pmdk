# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2023, Intel Corporation

"""Test framework utilities."""

from os.path import join, abspath
import os
import subprocess as sp

import consts as c
import configurator


def get_tool_path(ctx, name):
    return abspath(join(c.ROOTDIR, '..', 'tools', name, name))


def get_test_tool_path(build, name):
    return abspath(join(c.ROOTDIR, 'tools', name, name))


def get_lib_dir(ctx):
    if str(ctx.build) == 'debug':
        return c.DEBUG_LIBDIR
    else:
        return c.NONDEBUG_LIBDIR


def get_example_path(ctx, libname, name, dirname=None):
    """
    Get the path to the example binary.
    On Unix systems, the example binaries are located in the catalog
    "lib + libname/name" and have the same name as .c file.
    If that is not the case, dirname optional argument can be used to
    specify different catalog for the example binary- "lib + libname/dirname".
    """
    if dirname is None:
        dirname = name
    return abspath(join(c.ROOTDIR, '..', 'examples', 'lib' + libname,
                        dirname, name))


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


def run_command(cmd, errormsg=""):
    proc = sp.Popen(cmd, shell=True, stdout=sp.PIPE, stderr=sp.STDOUT)
    out = proc.communicate()

    if proc.returncode != 0:
        raise Fail("failed to execute command {}: {}".format(cmd, errormsg))
    return out[0]


class Color:
    """
    Set the font color. This functionality relies on ANSI escape sequences.
    """
    RED = '\33[91m'
    GREEN = '\33[92m'
    END = '\33[0m'


class Message:
    """Simple level based logger."""

    def __init__(self, level):
        self.level = level

    def print(self, msg):
        if self.level >= 1:
            print(msg)

    def print_verbose(self, msg):
        if self.level >= 2:
            print(msg)


class Fail(Exception):
    """Thrown when the test fails."""

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
    """Thrown when the test should be skipped."""

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
    """
    Translate provided keyword arguments into
    class attributes.
    """
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
