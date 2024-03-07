#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation
#


import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
# The 'nondebug' build is chosen arbitrarily to ensure these tests are run only
# once. No dynamic libraries are used nor .static_* builds are available.
@t.require_build('nondebug')
class CORE_LOG_MT(t.BaseTest):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('core_log_function_mt', self.test_case)


class SEL_CALL(CORE_LOG_MT):
    test_case = 'test_function_set_call'


@t.require_valgrind_enabled('helgrind')
class TEST0(SEL_CALL):
    pass


@t.require_valgrind_enabled('drd')
class TEST1(SEL_CALL):
    pass
