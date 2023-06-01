#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2023, Intel Corporation
#

import os
import testframework as t
from testframework import granularity as g
import re


@t.linux_only
@t.require_admin
@g.require_granularity(g.CACHELINE)
@t.require_ndctl(require_namespace=True)
@g.require_real_pmem()
class PMEM2_BADBLOCK_COUNT(t.Test):
    test_type = t.Short

    def run_test(self, ctx, filepath):
        old_log_level = ctx.env['UNITTEST_LOG_LEVEL']

        # writing to stdout file with UT_OUT requires log level >= 2
        if old_log_level is None or int(old_log_level) < 2:
            ctx.env['UNITTEST_LOG_LEVEL'] = '2'

        bbTool = ctx.badblock
        bbTool.clear_all(filepath)
        bbTool.inject(filepath, 0)

        log_file = 'bb.log'
        ctx.exec('pmem2_badblock', self.test_case, filepath,
                 stdout_file=log_file)

        bb_log_path = os.path.join(self.cwd, log_file)

        log_content = open(bb_log_path).read()
        bb_line = re.findall("BB: .*", log_content, re.MULTILINE).pop()

        # get only the log content after BB log header
        bb_log_header = "BB: "
        bb = int(bb_line[len(bb_log_header):])

        bb_from_tool = bbTool.get_count(filepath)
        if bb_from_tool != bb:
            t.futils.fail("BB count mismatch (tool: {}, pmem2: {})"
                          .format(bb_from_tool, bb))
        bbTool.clear_all(filepath)

        # restore previous log level
        ctx.env['UNITTEST_LOG_LEVEL'] = old_log_level


@t.linux_only
@t.require_admin
@g.require_granularity(g.CACHELINE)
@t.require_ndctl(require_namespace=True)
@g.require_real_pmem()
class PMEM2_BADBLOCK(t.Test):
    test_type = t.Short

    def run_test(self, ctx, filepath):
        bbTool = ctx.badblock
        bbTool.clear_all(filepath)
        bbTool.inject(filepath, 0)

        ctx.exec('pmem2_badblock', self.test_case, filepath)

        bbTool.clear_all(filepath)


# XXX - https://github.com/pmem/pmdk/issues/5636
class DISABLE_TEST0(PMEM2_BADBLOCK_COUNT):
    """
    compares the number of bad blocks returned by pmem2 and ndctl on fsdax
    """
    test_case = "test_pmem2_badblock_count"

    def run(self, ctx):
        filepath = ctx.create_holey_file(4 * t.KiB, 'testfile')
        self.run_test(ctx, filepath)


# XXX - https://github.com/pmem/pmdk/issues/5636
@t.require_devdax(t.DevDax('devdax1'))
class DISABLE_TEST1(PMEM2_BADBLOCK_COUNT):
    """
    compares the number of bad blocks returned by pmem2 and ndctl on devdax
    """
    test_case = "test_pmem2_badblock_count"

    def run(self, ctx):
        ddpath = ctx.devdaxes.devdax1.path
        self.run_test(ctx, ddpath)


# XXX - https://github.com/pmem/pmdk/issues/5636
class DISABLE_TEST2(PMEM2_BADBLOCK):
    """test mcsafe read operation with encountered badblock"""
    test_case = "test_pmem2_src_mcsafe_badblock_read"

    def run(self, ctx):
        filepath = ctx.create_holey_file(4 * t.KiB, 'testfile')
        self.run_test(ctx, filepath)


# XXX - https://github.com/pmem/pmdk/issues/5636
class DISABLE_TEST3(PMEM2_BADBLOCK):
    """test mcsafe write operation with encountered badblock"""
    test_case = "test_pmem2_src_mcsafe_badblock_write"

    def run(self, ctx):
        filepath = ctx.create_holey_file(4 * t.KiB, 'testfile')
        self.run_test(ctx, filepath)


# XXX - https://github.com/pmem/pmdk/issues/5636
@t.require_devdax(t.DevDax('devdax1'))
class DISABLE_TEST4(PMEM2_BADBLOCK):
    """test mcsafe read operation with encountered badblock on devdax"""
    test_case = "test_pmem2_src_mcsafe_badblock_read"

    def run(self, ctx):
        ddpath = ctx.devdaxes.devdax1.path
        self.run_test(ctx, ddpath)


# XXX - https://github.com/pmem/pmdk/issues/5636
@t.require_devdax(t.DevDax('devdax1'))
class DISABLE_TEST5(PMEM2_BADBLOCK):
    """test mcsafe write operation with encountered badblock on devdax"""
    test_case = "test_pmem2_src_mcsafe_badblock_write"

    def run(self, ctx):
        ddpath = ctx.devdaxes.devdax1.path
        self.run_test(ctx, ddpath)
