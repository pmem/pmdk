#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

import testframework as t
from testframework import granularity as g


@t.linux_only
@t.require_admin
@g.require_granularity(g.CACHELINE)
@t.require_ndctl(require_namespace=True)
@g.require_real_pmem()
class PMEMSET_BADBLOCK(t.Test):
    test_type = t.Short

    def run_test(self, ctx, filepath):
        bbTool = ctx.badblock
        bbTool.clear_all(filepath)
        bbTool.inject(filepath, 0)

        ctx.exec('pmemset_badblock', self.test_case, filepath)

        bbTool.clear_all(filepath)


class TEST0(PMEMSET_BADBLOCK):
    """test mcsafe read operation with encountered badblock"""
    test_case = "test_pmemset_src_mcsafe_badblock_read"

    def run(self, ctx):
        filepath = ctx.create_holey_file(4 * t.KiB, 'testfile')
        self.run_test(ctx, filepath)


class TEST1(PMEMSET_BADBLOCK):
    """test mcsafe write operation with encountered badblock"""
    test_case = "test_pmemset_src_mcsafe_badblock_write"

    def run(self, ctx):
        filepath = ctx.create_holey_file(4 * t.KiB, 'testfile')
        self.run_test(ctx, filepath)


@t.require_devdax(t.DevDax('devdax1'))
class TEST2(PMEMSET_BADBLOCK):
    """test mcsafe read operation with encountered badblock on devdax"""
    test_case = "test_pmemset_src_mcsafe_badblock_read"

    def run(self, ctx):
        ddpath = ctx.devdaxes.devdax1.path
        self.run_test(ctx, ddpath)


@t.require_devdax(t.DevDax('devdax1'))
class TEST3(PMEMSET_BADBLOCK):
    """test mcsafe write operation with encountered badblock on devdax"""
    test_case = "test_pmemset_src_mcsafe_badblock_write"

    def run(self, ctx):
        ddpath = ctx.devdaxes.devdax1.path
        self.run_test(ctx, ddpath)


class TEST4(PMEMSET_BADBLOCK):
    """test pmemset map on a source with a badblock"""
    test_case = "test_pmemset_map_detect_badblock"

    def run(self, ctx):
        filepath = ctx.create_holey_file(4 * t.KiB, 'testfile')
        self.run_test(ctx, filepath)


@t.require_devdax(t.DevDax('devdax1'))
class TEST5(PMEMSET_BADBLOCK):
    """test pmemset map on a source with a badblock on devdax"""
    test_case = "test_pmemset_map_detect_badblock"

    def run(self, ctx):
        ddpath = ctx.devdaxes.devdax1.path
        self.run_test(ctx, ddpath)


class TEST6(PMEMSET_BADBLOCK):
    """test pmemset map on a source with a badblock and clear it"""
    test_case = "test_pmemset_map_detect_badblock_and_clear"

    def run(self, ctx):
        filepath = ctx.create_holey_file(4 * t.KiB, 'testfile')
        self.run_test(ctx, filepath)


@t.require_devdax(t.DevDax('devdax1'))
class TEST7(PMEMSET_BADBLOCK):
    """test pmemset map on a source with a badblock and clear it on devdax"""
    test_case = "test_pmemset_map_detect_badblock_and_clear"

    def run(self, ctx):
        ddpath = ctx.devdaxes.devdax1.path
        self.run_test(ctx, ddpath)
