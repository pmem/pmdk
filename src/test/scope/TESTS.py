#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2023, Intel Corporation
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


@t.require_valgrind_disabled('drd', 'helgrind', 'memcheck', 'pmemcheck')
@g.no_testdir()
class Common(t.Test):
    test_type = t.Medium

    checked_lib = ''

    def run(self, ctx):
        static = False
        if str(self.ctx.build) in ['debug', 'release']:
            lib = '{}.so.1'.format(self.checked_lib)
        else:
            static = True
            lib = '{}.a'.format(self.checked_lib)

        libpath = os.path.join(ft.get_lib_dir(ctx), lib)

        log = 'out{}.log'.format(self.testnum)
        out = parse_lib(ctx, libpath, static)
        with open(os.path.join(self.cwd, log), 'w') as f:
            f.write(out)


class TEST2(Common):
    """Check scope of libpmem library (*nix)"""
    checked_lib = 'libpmem'


class TEST5(Common):
    """Check scope of libpmemobj library (*nix)"""
    checked_lib = 'libpmemobj'


class TEST6(Common):
    """Check scope of libpmempool library (*nix)"""
    checked_lib = 'libpmempool'


class TEST13(Common):
    """Check scope of libpmem2 library (*nix)"""
    checked_lib = 'libpmem2'
