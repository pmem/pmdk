#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation


import testframework as t
from testframework import granularity as g


class BASIC(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        ctx.exec('obj_heap_reopen', filepath)


@g.require_granularity(g.BYTE, g.CACHELINE)
class TEST0(BASIC):
    pass
