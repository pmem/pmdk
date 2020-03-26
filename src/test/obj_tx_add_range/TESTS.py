#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation

"""unit tests for pmemobj_tx_add_range and pmemobj_tx_xadd_range"""

from os import path

import testframework as t


@t.require_valgrind_disabled('memcheck', 'pmemcheck')
class TEST0(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        testfile = path.join(ctx.testdir, 'testfile0')
        ctx.exec('obj_tx_add_range', testfile, '0')


@t.require_valgrind_enabled('pmemcheck')
class TEST1(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        ctx.valgrind.add_opt('--mult-stores=no')

        testfile = path.join(ctx.testdir, 'testfile1')
        ctx.exec('obj_tx_add_range', testfile, '0')


@t.require_valgrind_disabled('memcheck')
class TEST2(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        testfile = path.join(ctx.testdir, 'testfile2')
        ctx.exec('obj_tx_add_range', testfile, '1')


@t.require_valgrind_enabled('memcheck')
@t.require_build('debug')
class TEST3(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        testfile = path.join(ctx.testdir, 'testfile3')
        ctx.exec('obj_tx_add_range', testfile, '0')
