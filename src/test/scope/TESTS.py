#!/usr/bin/env python3
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

#
# src/test/scope/TESTS.py -- scope tests to check libraries symbols
#

import os
import sys
import subprocess as sp

import futils as ft
import testframework as t


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
    dllview = ft.get_test_tool_path(ctx, 'dllview')
    cmd = [dllview, lib]
    proc = sp.run(cmd, universal_newlines=True,
                  stdout=sp.PIPE, stderr=sp.STDOUT)
    if proc.returncode != 0:
        raise ft.Fail('command "{}" failed: {}'
                      .format(' '.join(cmd), proc.stdout))

    out = sorted(proc.stdout.splitlines())
    return '\n'.join(out) + '\n'


@t.require_fs('non')
class Common(t.Test):
    test_type = t.Medium

    checked_lib = ''

    def run(self, ctx):
        static = False
        if sys.platform == 'win32':
            lib = '{}.dll'.format(self.checked_lib)
        elif str(self.ctx.build) in ['debug', 'release']:
            lib = '{}.so.1'.format(self.checked_lib)
        else:
            static = True
            lib = '{}.a'.format(self.checked_lib)

        libpath = os.path.join(ft.get_lib_dir(ctx), lib)

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
