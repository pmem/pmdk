#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation
#


import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
@t.require_build('nondebug')
@t.require_valgrind_disabled('pmemcheck', 'memcheck')
class CORE_LOG_MT(t.BaseTest):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('core_log_threshold_mt', self.test_case)


class TEST0(CORE_LOG_MT):
    test_case = 'test_threshold_set_get'


@t.require_valgrind_enabled('helgrind')
class TEST1(TEST0):
    pass


@t.require_valgrind_enabled('drd')
class TEST2(TEST0):
    pass


class TEST3(CORE_LOG_MT):
    test_case = 'test_threshold_aux_set_get'


@t.require_valgrind_enabled('helgrind')
class TEST4(TEST3):
    pass


@t.require_valgrind_enabled('drd')
class TEST5(TEST3):
    pass
