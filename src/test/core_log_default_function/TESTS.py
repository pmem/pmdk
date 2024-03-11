#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation
#


import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
@t.require_valgrind_disabled('pmemcheck')
# The 'debug' build is chosen arbitrarily to ensure these tests are run only
# once. No dynamic libraries are used nor .static_* builds are available.
@t.require_build('debug')
class CORE_LOG(t.BaseTest):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('core_log_default_function', self.test_case)


class TEST0(CORE_LOG):
    test_case = 'test_default_function'


class TEST1(CORE_LOG):
    test_case = 'test_default_function_bad_file_name'


class TEST2(CORE_LOG):
    test_case = 'test_default_function_short_file_name'


class TEST3(CORE_LOG):
    test_case = 'test_default_function_no_file_name'


class TEST4(CORE_LOG):
    test_case = 'test_default_function_no_function_name'


class TEST5(CORE_LOG):
    test_case = 'test_default_function_bad_timestamp'
