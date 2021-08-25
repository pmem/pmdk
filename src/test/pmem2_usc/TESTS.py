#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

import testframework as t
import re
import os
from testframework import granularity as g


@g.require_usc()
class Pmem2USC(t.Test):
    test_type = t.Short

    def run(self, ctx):
        old_log_level = ctx.env['UNITTEST_LOG_LEVEL']

        # writing to stdout file with UT_OUT requires log level >= 2
        if old_log_level is None or int(old_log_level) < 2:
            ctx.env['UNITTEST_LOG_LEVEL'] = '2'

        filepath = ctx.create_holey_file(1 * t.MiB, 'testfile')

        log_file = os.path.join(ctx.testdir, 'usc.log')
        ctx.exec('pmem2_usc', filepath, stdout_file=log_file)

        log_content = open(log_file).read()
        usc_line = re.findall("USC: .*", log_content, re.MULTILINE).pop()

        # get only the log content after USC log header
        usc_log_header = "USC: "
        usc = int(usc_line[len(usc_log_header):])
        usc_from_tool = ctx.usc.read(ctx.testdir)

        if usc_from_tool != usc:
            t.futils.fail("USC mismatch (tool: {}, pmem2: {})"
                          .format(usc_from_tool, usc))

        # restore previous log level
        ctx.env['UNITTEST_LOG_LEVEL'] = old_log_level


@t.linux_only
class TEST0(Pmem2USC):
    """check if pmem2 usc output is the same as ndctl/impctl output"""
