#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation
#


import sys

import testframework as t
from testframework import granularity as g
from consts import MINIASYNC_LIBDIR


class PMEM2_MOVER(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile')
        ctx.exec('pmem2_mover', self.test_case, filepath)


@g.require_granularity(g.BYTE, g.CACHELINE)
class PMEM2_MOVER_MT(t.Test):
    test_type = t.Medium
    thread_num = 2

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile')
        ctx.exec('pmem2_mover', self.test_case, filepath, self.thread_num)


class TEST0(PMEM2_MOVER):
    """veryfy pmem2 mover functionality"""
    test_case = "test_mover_basic"


class TEST1(PMEM2_MOVER_MT):
    """veryfy pmem2 mover multithreaded functionality"""
    test_case = "test_mover_multithreaded"


class TEST3(PMEM2_MOVER):
    """veryfy pmem2 mover functionality"""
    test_case = "test_miniasync_mover"

    def setup(self, ctx):
        super().setup(ctx)
        if sys.platform == 'win32':
            env_dir = 'PATH'
        else:
            env_dir = 'LD_LIBRARY_PATH'

        path = ctx.env[env_dir]
        ctx.env[env_dir] = path + ":" + MINIASYNC_LIBDIR
        print(ctx.env[env_dir])
