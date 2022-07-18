#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation


import testframework as t


class BASIC(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        ctx.exec('obj_heap_reopen', filepath)


class TEST0(BASIC):
    pass
