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
        ctx.exec('core_log_default_function', self.test_case)


class TEST0(CORE_LOG):
    test_case = 'test_1'


class TEST1(CORE_LOG):
    test_case = 'test_CORE_LOG_TRESHOLD'
