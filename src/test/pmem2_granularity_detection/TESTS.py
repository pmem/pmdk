#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2024, Intel Corporation
#


import testframework as t
from testframework import granularity as g
import futils as f


@g.require_granularity(g.ANY)
class PMEM2_CONFIG(t.Test):
    test_type = t.Short


class TEST0(PMEM2_CONFIG):
    """test granularity detection with PMEM2_FORCE_GRANULARITY set to page"""
    def run(self, ctx):
        ctx.env['PMEM2_FORCE_GRANULARITY'] = "page"
        ctx.exec(f.get_test_tool_path(ctx.build, "gran_detecto"),
                 '-p', ctx.testdir)


class TEST1(PMEM2_CONFIG):
    """test granularity detection with PMEM2_FORCE_GRANULARITY
    set to cache_line"""
    def run(self, ctx):
        ctx.env['PMEM2_FORCE_GRANULARITY'] = "cache_line"
        ctx.exec(f.get_test_tool_path(ctx.build, "gran_detecto"),
                 '-c', ctx.testdir)


class TEST2(PMEM2_CONFIG):
    """test granularity detection with PMEM2_FORCE_GRANULARITY set to byte"""
    def run(self, ctx):
        ctx.env['PMEM2_FORCE_GRANULARITY'] = "byte"
        ctx.exec(f.get_test_tool_path(ctx.build, "gran_detecto"),
                 '-b', ctx.testdir)


class TEST3(PMEM2_CONFIG):
    """test granularity detection with PMEM2_FORCE_GRANULARITY
    set to CaCHe_Line"""
    def run(self, ctx):
        ctx.env['PMEM2_FORCE_GRANULARITY'] = "CaCHe_Line"
        ctx.exec(f.get_test_tool_path(ctx.build, "gran_detecto"),
                 '-c', ctx.testdir)


class TEST4(PMEM2_CONFIG):
    """test granularity detection with PMEM2_FORCE_GRANULARITY
    set to CACHELINE"""
    def run(self, ctx):
        ctx.env['PMEM2_FORCE_GRANULARITY'] = "CACHELINE"
        ctx.exec(f.get_test_tool_path(ctx.build, "gran_detecto"),
                 '-c', ctx.testdir)
