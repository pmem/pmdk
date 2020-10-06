#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
class PmemsetPerror(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmemset_perror', self.test_case,
                 stderr_file=self.stderr_file)


class TEST0(PmemsetPerror):
    test_case = 'test_fail_pmemset_func_simple'
    stderr_file = 'test0.log'


class TEST1(PmemsetPerror):
    test_case = 'test_fail_pmemset_func_format'
    stderr_file = 'test1.log'


class TEST2(PmemsetPerror):
    test_case = 'test_fail_system_func_simple'
    stderr_file = 'test2.log'


class TEST3(PmemsetPerror):
    test_case = 'test_fail_system_func_format'
    stderr_file = 'test3.log'


class TEST4(PmemsetPerror):
    test_case = 'test_pmemset_err_to_errno_simple'
    stderr_file = 'test4.log'
