#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
class PMEMSET_PERROR(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmemset_perror', self.test_case,
                 stderr_file=self.stderr_file)


class TEST0(PMEMSET_PERROR):
    """check print message when func from pmemset API fails"""
    test_case = 'test_fail_pmemset_func_simple'
    stderr_file = 'test0.log'


class TEST1(PMEMSET_PERROR):
    """
    check print message when func from pmemset API fails and ellipsis
    operator is used
    """
    test_case = 'test_fail_pmemset_func_format'
    stderr_file = 'test1.log'


class TEST2(PMEMSET_PERROR):
    """check print message when directly called system func fails"""
    test_case = 'test_fail_system_func_simple'
    stderr_file = 'test2.log'


class TEST3(PMEMSET_PERROR):
    """
    check print message when directly called system func fails and ellipsis
    operator is used
    """
    test_case = 'test_fail_system_func_format'
    stderr_file = 'test3.log'


class TEST4(PMEMSET_PERROR):
    """check if conversion from pmemset err value to errno works fine"""
    test_case = 'test_pmemset_err_to_errno_simple'
    stderr_file = 'test4.log'
