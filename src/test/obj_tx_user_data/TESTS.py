#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2024, Intel Corporation
#


import testframework as t


class TEST0(t.Test):
    test_type = t.Short

    def run(self, ctx):
        testfile = t.path.join(ctx.testdir, 'testfile')
        ctx.exec('obj_tx_user_data', testfile)
