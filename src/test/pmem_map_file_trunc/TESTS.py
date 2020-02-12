#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation


import testframework as t
import os


@t.require_build(['debug', 'release'])
class TEST0(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem_map_file_trunc', os.path.join(ctx.testdir, 'testfile'))
