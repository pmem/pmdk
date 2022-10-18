#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#

#
# src/test/scope/TESTS.py -- scope tests to check libraries symbols
#

import os
import sys
import subprocess as sp

import futils as ft
import testframework as t
from testframework import granularity as g


def parse_lib(ctx, lib, static=False):
    if sys.platform.startswith('linux'):
        return parse_lib_linux(ctx, lib, static)
    elif sys.platform == 'win32':
        return parse_lib_win(ctx, lib, static)


def parse_lib_linux(ctx, lib, static):
    if static:
        arg = '-g'
    else:
        arg = '-D'
    cmd = ['nm', arg, lib]
    proc = sp.run(cmd,
                  universal_newlines=True, stdout=sp.PIPE, stderr=sp.STDOUT)
    if proc.returncode != 0:
        raise ft.Fail('command "{}" failed: {}'
                      .format(' '.join(cmd), proc.stdout))

    symbols = []
    for line in proc.stdout.splitlines():
        try:
            # penultimate column of 'nm' output must be either
            # 'T' (defined function) or 'B' (global variable).
            # Example output lines:
            #     000000000003fde4 T pmemobj_create
            #     0000000000000018 B _pobj_cached_pool
            #                      U read
            if line.split()[-2] in 'TB':
                symbols.append(line.split()[-1] + os.linesep)
        except IndexError:
            continue

    symbols.sort()
    return ''.join(symbols)


def parse_lib_win(ctx, lib, static):
    dllview = ft.get_test_tool_path(ctx.build, 'dllview') + '.exe'
    cmd = [dllview, lib]
    proc = sp.run(cmd, universal_newlines=True,
                  stdout=sp.PIPE, stderr=sp.STDOUT)
    if proc.returncode != 0:
        raise ft.Fail('command "{}" failed: {}'
                      .format(' '.join(cmd), proc.stdout))

    out = sorted(proc.stdout.splitlines())
    return '\n'.join(out) + '\n'


@t.require_valgrind_disabled('drd', 'helgrind', 'memcheck', 'pmemcheck')
@g.no_testdir()
class Common(t.Test):
    test_type = t.Medium

    checked_lib = ''

    def run(self, ctx):
        static = False
        if sys.platform == 'win32':
            lib = '{}.dll'.format(self.checked_lib)
        elif str(self.ctx.build) in ['debug', 'release', 'debugoptimized']:
            lib = '{}.so.1'.format(self.checked_lib)
        else:
            static = True
            lib = '{}.a'.format(self.checked_lib)

        libdirs = ft.get_lib_dir(ctx).split(':')
        libdir = list(filter(lambda l: self.checked_lib == l.split('/')[-1],
                             libdirs))[0]
        libpath = os.path.join(libdir, lib)

        log = 'out{}.log'.format(self.testnum)
        out = parse_lib(ctx, libpath, static)
        with open(os.path.join(self.cwd, log), 'w') as f:
            f.write(out)


@t.windows_exclude
class TEST2(Common):
    """Check scope of libpmem library (*nix)"""
    checked_lib = 'libpmem'


@t.windows_exclude
class TEST3(Common):
    """Check scope of libpmemlog library (*nix)"""
    checked_lib = 'libpmemlog'


@t.windows_exclude
class TEST4(Common):
    """Check scope of libpmemblk library (*nix)"""
    checked_lib = 'libpmemblk'


@t.windows_exclude
class TEST5(Common):
    """Check scope of libpmemobj library (*nix)"""
    checked_lib = 'libpmemobj'


@t.windows_exclude
class TEST6(Common):
    """Check scope of libpmempool library (*nix)"""
    checked_lib = 'libpmempool'


@t.windows_only
class TEST8(Common):
    """Check scope of libpmem library (windows)"""
    checked_lib = 'libpmem'


@t.windows_only
class TEST9(Common):
    """Check scope of libpmemlog library (windows)"""
    checked_lib = 'libpmemlog'


@t.windows_only
class TEST10(Common):
    """Check scope of libpmemblk library (windows)"""
    checked_lib = 'libpmemblk'


@t.windows_only
class TEST11(Common):
    """Check scope of libpmemobj library (windows)"""
    checked_lib = 'libpmemobj'


@t.windows_only
class TEST12(Common):
    """Check scope of libpmempool library (windows)"""
    checked_lib = 'libpmempool'


@t.windows_exclude
class TEST13(Common):
    """Check scope of libpmem2 library (*nix)"""
    checked_lib = 'libpmem2'


@t.windows_only
class TEST14(Common):
    """Check scope of libpmem2 library (windows)"""
    checked_lib = 'libpmem2'


@t.windows_exclude
class TEST15(Common):
    """Check scope of libpmemset library (*nix)"""
    checked_lib = 'libpmemset'


@t.windows_only
class TEST16(Common):
    """Check scope of libpmemset library (windows)"""
    checked_lib = 'libpmemset'
