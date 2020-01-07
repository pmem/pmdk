#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#

from os import path
import testframework as t


class BASE(t.BaseTest):
    test_type = t.Medium

    def run(self, ctx):
        testfile = path.join(ctx.testdir, 'testfile0')
        ctx.exec('obj_defrag', testfile)


class TEST0(BASE):
    "defrag test"
