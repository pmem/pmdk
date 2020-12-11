#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#


import testframework as t


@t.require_ndctl
@t.windows_exclude
class TEST0(t.Test):
    test_type = t.Short

    def run(self, ctx):
        testfile1 = ctx.create_holey_file(2 * t.MiB, 'testfile1')
        ctx.exec('pmem2_source_numa', 'test_get_numa_node',
                 testfile1, 0)


@t.require_ndctl
@t.windows_exclude
@t.require_devdax(t.DevDax('devdax', alignment=2 * t.MiB))
class TEST1(t.Test):
    test_type = t.Short

    def run(self, ctx):
        dd = ctx.devdaxes.devdax
        ctx.exec('pmem2_source_numa', 'test_get_numa_node',
                 dd.path, 0)
