#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
class PmemsetEvent(t.Test):
    test_type = t.Short
    filesize = 8 * t.MiB

    def run(self, ctx):
        filepath = ctx.create_holey_file(self.filesize, 'testfile',)
        ctx.exec('pmemset_event', self.test_case, filepath)


class TEST1(PmemsetEvent):
    test_case = "test_pmemset_persist_event"


class TEST2(PmemsetEvent):
    test_case = "test_pmemset_copy_event"
