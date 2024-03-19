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
class PMEM_LOG(t.BaseTest):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem_log_get_treshold', self.test_case)


class TEST0(PMEM_LOG):
    test_case = 'test_log_get_treshold'


class TEST1(PMEM_LOG):
    test_case = 'test_log_get_treshold_EAGAIN'
