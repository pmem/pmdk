#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#


import testframework as t


class PMEM2_MOVER(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile')
        ctx.exec('pmem2_mover', self.test_case, filepath)


class TEST0(PMEM2_MOVER):
    """veryfy pmem2 mover functionality"""
    test_case = "test_mover_basic"
