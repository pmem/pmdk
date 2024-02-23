#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation
#


import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
# @t.require_build('nondebug')
class CORE_LOG(t.BaseTest):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('core_log_last', self.test_case)


class TEST0(CORE_LOG):
    test_case = 'test_CORE_LOG_BASIC'


class TEST1(CORE_LOG):
    test_case = 'test_CORE_LOG_BASIC_W_ERRNO'


class TEST2(CORE_LOG):
    test_case = 'test_CORE_LOG_BASIC_W_ERRNO_BAD'


class TEST3(CORE_LOG):
    test_case = 'test_CORE_LOG_BASIC_LONG'


class TEST4(CORE_LOG):
    test_case = 'test_CORE_LOG_BASIC_TOO_LONG'


class TEST5(CORE_LOG):
    test_case = 'test_CORE_LOG_BASIC_TOO_LONG_W_ERRNO'


class TEST6(CORE_LOG):
    test_case = 'test_CORE_LOG_LAST_BASIC_LONG'


class TEST7(CORE_LOG):
    test_case = 'test_CORE_LOG_LAST_BASIC_TOO_LONG'


class TEST8(CORE_LOG):
    test_case = 'test_CORE_LOG_TRESHOLD'
