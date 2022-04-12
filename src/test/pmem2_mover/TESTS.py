#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation
#


import sys

import testframework as t
from testframework import granularity as g
from consts import MINIASYNC_LIBDIR


@t.freebsd_exclude
class PMEM2_MOVER(t.Test):
    test_type = t.Medium

    def setup(self, ctx):
        super().setup(ctx)
        if sys.platform == 'win32':
            env_dir = 'PATH'
            path = ctx.env[env_dir]
            ctx.env[env_dir] = path + ";" + MINIASYNC_LIBDIR
        else:
            env_dir = 'LD_LIBRARY_PATH'
            path = ctx.env[env_dir]
            ctx.env[env_dir] = path + ":" + MINIASYNC_LIBDIR
        self.filepath = ctx.create_holey_file(16 * t.MiB, 'testfile')

    def run(self, ctx):
        ctx.exec('pmem2_mover', self.test_case, self.filepath)


@g.require_granularity(g.BYTE, g.CACHELINE)
class PMEM2_MOVER_MT(PMEM2_MOVER):
    thread_num = 2

    def run(self, ctx):
        ctx.exec('pmem2_mover', self.test_case, self.filepath, self.thread_num)


class TEST0(PMEM2_MOVER):
    """verify pmem2 mover functionality"""
    test_case = "test_mover_basic"


class TEST1(PMEM2_MOVER_MT):
    """verify pmem2 mover multi-threaded functionality"""
    test_case = "test_mover_multithreaded"


class TEST2(PMEM2_MOVER_MT):
    test_type = t.Long
    thread_num = 16
    """verify pmem2 mover multi-threaded functionality (Long)"""
    test_case = "test_mover_multithreaded"


class TEST3(PMEM2_MOVER):
    """verify pmem2 mover functionality"""
    test_case = "test_miniasync_mover"
