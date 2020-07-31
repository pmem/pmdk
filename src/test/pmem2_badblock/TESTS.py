#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t
from testframework import granularity as g
import re


@t.linux_only
@t.require_ndctl
@t.require_admin
@g.require_granularity(g.CACHELINE)
class TEST0(t.Test):
    """compares the number of bad blocks returned by pmem2 and ndctl"""
    def run(self, ctx):
        bbTool = ctx.badblock

        test = 'test_pmem2_badblock_count'
        filepath = ctx.create_holey_file(1 * t.MiB, 'testfile')

        bbTool.inject(filepath, 0)

        log_file = 'bb.log'
        ctx.exec('pmem2_badblock', test, filepath, stdout_file=log_file)

        log_content = open(log_file).read()
        bb_line = re.findall("BB: .*", log_content, re.MULTILINE).pop()
        bb = int(bb_line[4:])
        bb_from_tool = bbTool.get_count(filepath)

        if bb_from_tool != bb:
            t.futils.fail("BB count mismatch (tool: {}, pmem2: {})"
                          .format(bb_from_tool, bb))

        bbTool.clear_all(filepath)
