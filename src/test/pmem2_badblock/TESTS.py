#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t
import re


@t.linux_only
@t.require_ndctl
@t.require_admin
class TEST0(t.Test):
    """counts"""
    def run(self, ctx):
        bbTool = ctx.badblock()

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
            t.futils.fail("BB count mismatch")

        bbTool.clear_all(filepath)
