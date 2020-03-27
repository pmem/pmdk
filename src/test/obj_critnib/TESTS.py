#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t
from testframework import granularity as g


@g.no_testdir()
class TEST0(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        ctx.exec('obj_critnib')
