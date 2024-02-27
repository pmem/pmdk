#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation
#


import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
class CORE_LOG(t.BaseTest):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('core_log_internal', self.test_case)


class TEST0(CORE_LOG):
    test_case = 'test_CORE_LOG_INTERNAL'


class TEST1(CORE_LOG):
    test_case = 'test_CORE_LOG_INTERNAL_LAST_MESSAGE'


class TEST2(CORE_LOG):
    test_case = 'test_CORE_LOG_INTERNAL_LAST_MESSAGE_W_ERRNO'


class TEST3(CORE_LOG):
    test_case = 'test_CORE_LOG_INTERNAL_W_ERRNO'


class TEST4(CORE_LOG):
    test_case = 'test_CORE_LOG_INTERNAL_TRESHOLD'
