#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t
from testframework import granularity as g


@t.require_fs(granularity=g.ANY)
class Pmem2Perror(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_perror', self.test_case, stderr_file=self.stderr_file)


class TEST0(Pmem2Perror):
    test_case = 'test_fail_pmem2_func_simple'
    stderr_file = 'test0.log'


class TEST1(Pmem2Perror):
    test_case = 'test_fail_pmem2_func_format'
    stderr_file = 'test1.log'


class TEST2(Pmem2Perror):
    test_case = 'test_fail_system_func_simple'
    stderr_file = 'test2.log'


class TEST3(Pmem2Perror):
    test_case = 'test_fail_system_func_format'
    stderr_file = 'test3.log'


class TEST4(Pmem2Perror):
    test_case = 'test_fail_pmem2_syscall_simple'
    stderr_file = 'test4.log'


class TEST5(Pmem2Perror):
    test_case = 'test_fail_pmem2_syscall_format'
    stderr_file = 'test5.log'
