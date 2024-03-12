#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation
#


import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
# The 'debug' build is chosen arbitrarily to ensure these tests are run only
# once. No dynamic libraries are used nor .static_* builds are available.
@t.require_build('debug')
class CORE_LOG(t.BaseTest):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('obj_log_function', self.test_case)


class TEST0(CORE_LOG):
    test_case = 'test_log_set_function'


class TEST1(CORE_LOG):
    test_case = 'test_log_set_function_EAGAIN'


class TEST2(CORE_LOG):
    test_case = 'test_log_set_treshold'


class TEST3(CORE_LOG):
    test_case = 'test_log_set_treshold_EAGAIN'


class TEST4(CORE_LOG):
    test_case = 'test_log_set_treshold_EINVAL'


class TEST5(CORE_LOG):
    test_case = 'test_log_get_treshold'


class TEST6(CORE_LOG):
    test_case = 'test_log_get_treshold_EAGAIN'
