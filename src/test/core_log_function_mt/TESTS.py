#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation
#


import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
@t.require_valgrind_disabled('pmemcheck', 'memcheck')
# The 'nondebug' build is chosen arbitrarily to ensure these tests are run only
# once. No dynamic libraries are used nor .static_* builds are available.
@t.require_build('nondebug')
class TEST0(t.BaseTest):
    test_type = t.Short
    test_case = 'test_function_set_call'

    def run(self, ctx):
        ctx.exec('core_log_function_mt', self.test_case)


@t.require_valgrind_enabled('helgrind')
class TEST1(TEST0):
    pass


@t.require_valgrind_enabled('drd')
class TEST2(TEST0):
    pass
