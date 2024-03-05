#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation
#


import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
@t.require_build('nondebug')
class CORE_LOG_MT(t.BaseTest):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('core_log_mt', self.test_case)


class THRESHOLD(CORE_LOG_MT):
    test_case = 'test_threshold'


class THRESHOLD_AUX(CORE_LOG_MT):
    test_case = 'test_threshold_aux'


@t.require_valgrind_enabled('helgrind')
class TEST0(THRESHOLD):
    pass


@t.require_valgrind_enabled('drd')
class TEST1(THRESHOLD):
    pass


@t.require_valgrind_enabled('helgrind')
class TEST2(THRESHOLD_AUX):
    pass


@t.require_valgrind_enabled('drd')
class TEST3(THRESHOLD_AUX):
    pass
