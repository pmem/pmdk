#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2023, Intel Corporation
#


import testframework as t


class TEST0(t.Test):
    test_type = t.Short

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile')
        ctx.exec('pmem2_source_alignment',
                 'test_get_alignment_success', filepath)


class PMEM2_SOURCE_ALIGNMENT_DEV_DAX(t.Test):
    test_type = t.Short
    test_case = "test_get_alignment_success"

    def run(self, ctx):
        dd = ctx.devdaxes.devdax
        ctx.exec('pmem2_source_alignment',
                 self.test_case, dd.path, dd.alignment)


@t.require_devdax(t.DevDax('devdax', alignment=2 * t.MiB))
class TEST1(PMEM2_SOURCE_ALIGNMENT_DEV_DAX):
    pass


@t.require_devdax(t.DevDax('devdax', alignment=4 * t.KiB))
class TEST2(PMEM2_SOURCE_ALIGNMENT_DEV_DAX):
    pass
