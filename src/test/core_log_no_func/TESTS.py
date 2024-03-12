#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation
#


import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
@t.require_valgrind_disabled('pmemcheck')
# The 'nondebug' build is chosen arbitrarily to ensure these tests are run only
# once. No dynamic libraries are used nor .static_* builds are available.
@t.require_build('nondebug')
class CORE_LOG(t.BaseTest):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('core_log_no_func', self.test_case)


class TEST0(CORE_LOG):
    test_case = 'test_no_log_function'


class TEST1(CORE_LOG):
    test_case = 'test_no_log_function_CORE_LOG_LEVEL_ERROR_LAST'
