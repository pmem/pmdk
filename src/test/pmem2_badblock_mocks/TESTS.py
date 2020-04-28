#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#

import testframework as t


@t.linux_only
class TEST0(t.Test):
    def run(self, ctx):
        ctx.exec('pmem2_badblock_mocks')
