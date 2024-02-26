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
    test_case = 'testDefaultFunction'


class TEST1(CORE_LOG):
    test_case = 'testDefaultFunctionBadFileInfo'


class TEST2(CORE_LOG):
    test_case = 'testDefaultFunctionShortFileName'


class TEST3(CORE_LOG):
    test_case = 'testDefaultFunctionNoFileName'


class TEST4(CORE_LOG):
    test_case = 'testDefaultFunctionBadTimestamp'
