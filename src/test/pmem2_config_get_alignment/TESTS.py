#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#


import testframework as t
from testframework import granularity as g


@g.no_testdir()
class TEST0(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_config_get_alignment', 'test_notset_fd')


class TEST1(t.Test):
    test_type = t.Short

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile')
        ctx.exec('pmem2_config_get_alignment',
                 'test_get_alignment_success', filepath)


@t.windows_exclude
@g.require_granularity(g.ANY)
class TEST2(t.BaseTest):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_config_get_alignment', 'test_directory',
                 ctx.testdir)


class PMEM2_CONFIG_GET_ALIGNMENT_DEV_DAX(t.Test):
    test_type = t.Short
    test_case = "test_get_alignment_success"

    def run(self, ctx):
        dd = ctx.devdaxes.devdax
        ctx.exec('pmem2_config_get_alignment',
                 self.test_case, dd.path, str(dd.alignment))


@t.windows_exclude
@t.require_devdax(t.DevDax('devdax', alignment=2 * t.MiB))
class TEST3(PMEM2_CONFIG_GET_ALIGNMENT_DEV_DAX):
    pass


@t.windows_exclude
@t.require_devdax(t.DevDax('devdax', alignment=4 * t.KiB))
class TEST4(PMEM2_CONFIG_GET_ALIGNMENT_DEV_DAX):
    pass
