#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#
import testframework as t
from testframework import granularity as g


@g.no_testdir()
class PMEM2_DEEP_FLUSH(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_deep_flush', self.test_case)


class TEST0(PMEM2_DEEP_FLUSH):
    """test pmem2_deep_flush"""
    test_case = "test_deep_flush_func"


@t.linux_only
class TEST1(PMEM2_DEEP_FLUSH):
    """test pmem2_deep_flush with mocked DAX devices"""
    test_case = "test_deep_flush_func_devdax"


class TEST2(PMEM2_DEEP_FLUSH):
    """test pmem2_deep_flush with range beyond mapping"""
    test_case = "test_deep_flush_range_beyond_mapping"
