#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
class PMEMSET_PART(t.Test):
    test_type = t.Short
    filepath = "/default/path"

    def run(self, ctx):
        ctx.exec('pmemset_part', self.test_case, self.filepath)


class TEST0(PMEMSET_PART):
    test_case = "test_part_new_invalid_source_path"
    filepath = "definitely/invalid/path/for/sure"
