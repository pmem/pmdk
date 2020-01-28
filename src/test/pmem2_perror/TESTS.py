#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
class TEST0(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_perror', 'test_simple_check', stderr_file='test0.log')


@g.require_granularity(g.ANY)
class TEST1(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_perror', 'test_format_check', stderr_file='test1.log')


@g.require_granularity(g.ANY)
class TEST2(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_perror', 'test_fail_pmem2_func_simple',
                 stderr_file='test2.log')


@g.require_granularity(g.ANY)
class TEST3(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_perror', 'test_fail_pmem2_func_format',
                 stderr_file='test3.log')


@g.require_granularity(g.ANY)
class TEST4(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_perror', 'test_fail_system_func_simple',
                 stderr_file='test4.log')


@g.require_granularity(g.ANY)
class TEST5(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_perror', 'test_fail_system_func_format',
                 stderr_file='test5.log')
