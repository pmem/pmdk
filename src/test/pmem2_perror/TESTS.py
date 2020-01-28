#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
class PMEM2_PERROR(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_perror', self.test_case, stderr_file=self.stderr_file)


class TEST0(PMEM2_PERROR):
    test_case = 'test_fail_pmem2_func_simple'
    stderr_file = 'test0.log'


class TEST1(PMEM2_PERROR):
    test_case = 'test_fail_pmem2_func_format'
    stderr_file = 'test1.log'


class TEST2(PMEM2_PERROR):
    test_case = 'test_fail_system_func_simple'
    stderr_file = 'test2.log'


class TEST3(PMEM2_PERROR):
    test_case = 'test_fail_system_func_format'
    stderr_file = 'test3.log'


class TEST4(PMEM2_PERROR):
    test_case = 'test_fail_pmem2_syscall_simple'
    stderr_file = 'test4.log'


class TEST5(PMEM2_PERROR):
    test_case = 'test_fail_pmem2_syscall_format'
    stderr_file = 'test5.log'
