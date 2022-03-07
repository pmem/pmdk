#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation
#


import testframework as t
from testframework import granularity as g


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
    """veryfy pmem2 mover multithread functionality"""
    test_case = "test_mover_multihread"


class TEST2(PMEM2_MOVER_MT):
    """veryfy pmem2 mover multithread functionality (long)"""
    test_case = "test_mover_multihread"
    test_type = t.Long
    thread_num = 32
