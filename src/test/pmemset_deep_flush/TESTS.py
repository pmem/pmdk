#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
class PMEMSET_DEEP_FLUSH(t.Test):
    test_type = t.Short
    create_file = True

    def run(self, ctx):
        filepath = ctx.create_holey_file(32 * t.MiB, 'testfile1')
        ctx.exec('pmemset_deep_flush', self.test_case, filepath)


class TEST0(PMEMSET_DEEP_FLUSH):
    """test pmemset_deep_flush on single part map"""
    test_case = "test_deep_flush_single"


class TEST1(PMEMSET_DEEP_FLUSH):
    """test pmemset_deep_flush on multiple part maps"""
    test_case = "test_deep_flush_multiple"


class TEST2(PMEMSET_DEEP_FLUSH):
    """test pmemset_deep_flush on multiple part maps with coalescing"""
    test_case = "test_deep_flush_multiple_coal"
