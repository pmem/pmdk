#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2024, Intel Corporation

"""
unit tests for pmemobj_tx_add_range_direct
and pmemobj_tx_xadd_range_direct
"""

from os import path

import testframework as t


@t.require_valgrind_disabled('pmemcheck', 'memcheck')
class TEST0(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        testfile = path.join(ctx.testdir, 'testfile0')
        ctx.exec('obj_tx_add_range_direct', testfile)


@t.require_valgrind_enabled('pmemcheck')
class TEST1(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        ctx.valgrind.add_opt('--mult-stores=no')

        testfile = path.join(ctx.testdir, 'testfile1')
        ctx.exec('obj_tx_add_range_direct', testfile)


@t.require_valgrind_enabled('memcheck')
@t.require_build('debug')
class TEST2(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        testfile = path.join(ctx.testdir, 'testfile2')
        ctx.exec('obj_tx_add_range_direct', testfile)
