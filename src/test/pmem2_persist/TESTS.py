#!../env.py
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation

import testframework as t
from testframework import granularity as g


@g.no_testdir()
class PMEM2_PERSIST(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_persist', self.test_case)


class TEST0(PMEM2_PERSIST):
    """test getting pmem2 persist functions"""
    test_case = "test_get_persist_funcs"


class TEST1(PMEM2_PERSIST):
    """test getting pmem2 flush functions"""
    test_case = "test_get_flush_funcs"


class TEST2(PMEM2_PERSIST):
    """test getting pmem2 drain functions"""
    test_case = "test_get_drain_funcs"
