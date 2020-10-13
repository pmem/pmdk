#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
class PMEMSET_SOURCE(t.Test):
    test_type = t.Short
    filepath = "default"

    def run(self, ctx):
        ctx.exec('pmemset_source', self.test_case, self.filepath)


class TEST0(PMEMSET_SOURCE):
    """check if file path is saved in structure"""
    test_case = "test_source_from_file"
    filepath = "/example/path"
