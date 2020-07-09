#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t
import re
import os
from testframework import granularity as g


@g.require_usc()
class Pmem2USC(t.Test):
    test_type = t.Short

    def run(self, ctx):
        filepath = ctx.create_holey_file(1 * t.MiB, 'testfile')

        log_file = os.path.join(ctx.testdir, '/usc.log')
        ctx.exec('pmem2_usc', filepath, stdout_file=log_file)

        log_content = open(log_file).read()
        usc_line = re.findall("USC: .*", log_content, re.MULTILINE).pop()
        usc = int(usc_line[5:])
        usc_from_tool = ctx.usc.read(ctx.testdir)

        if usc_from_tool != usc:
            t.futils.fail("USC mismatch (tool: {}, pmem2: {})"
                          .format(usc_from_tool, usc))


@t.linux_only
class TEST0(Pmem2USC):
    """check if pmem2 usc output is the same as ndctl/impctl output"""
