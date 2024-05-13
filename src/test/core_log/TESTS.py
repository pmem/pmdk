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
        ctx.exec('core_log', self.test_case)


class TEST0(CORE_LOG):
    test_case = 'test_CORE_LOG_LEVEL_ERROR_LAST'


class TEST1(CORE_LOG):
    test_case = 'test_vsnprintf_fail'


class TEST2(CORE_LOG):
    test_case = 'test_NO_ERRNO'


class TEST3(CORE_LOG):
    test_case = 'test_no_space_for_strerror_r'


class TEST4(CORE_LOG):
    test_case = 'test_strerror_r_fail'


class TEST5(CORE_LOG):
    test_case = 'test_level_gt_threshold'


class TEST6(CORE_LOG):
    test_case = 'test_happy_day'


class TEST7(CORE_LOG):
    test_case = 'test_set_custom_function'
