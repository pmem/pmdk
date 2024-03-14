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
class OBJ_LOG(t.BaseTest):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('obj_log_set_treshold', self.test_case)


class TEST0(OBJ_LOG):
    test_case = 'test_log_set_treshold'


class TEST1(OBJ_LOG):
    test_case = 'test_log_set_treshold_EAGAIN'


class TEST2(OBJ_LOG):
    test_case = 'test_log_set_treshold_EINVAL'
