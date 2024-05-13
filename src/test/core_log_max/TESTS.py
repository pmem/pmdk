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
        ctx.exec('core_log_max', self.test_case)


class TEST0(CORE_LOG):
    test_case = 'test_CORE_LOG_MAX_ERRNO_MSG'


class TEST1(CORE_LOG):
    test_case = 'test_ERR_W_ERRNO'


class TEST2(CORE_LOG):
    test_case = 'test_CORE_LOG'
