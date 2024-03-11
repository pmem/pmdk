#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation
#


import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
@t.require_build('nondebug')
@t.require_valgrind_disabled('pmemcheck')
class CORE_LOG(t.BaseTest):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('core_log_internal', self.test_case)


class TEST0(CORE_LOG):
    test_case = 'test_CORE_LOG'


class TEST1(CORE_LOG):
    test_case = 'test_CORE_LOG_ERROR_LAST'


class TEST2(CORE_LOG):
    test_case = 'test_CORE_LOG_ERROR_W_ERRNO_LAST'


class TEST3(CORE_LOG):
    test_case = 'test_CORE_LOG_W_ERRNO'


class TEST4(CORE_LOG):
    test_case = 'test_CORE_LOG_TRESHOLD'


class TEST5(CORE_LOG):
    test_case = 'test_CORE_LOG_TRESHOLD_DEFAULT'
