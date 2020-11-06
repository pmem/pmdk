#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
class PMEMSET_SOURCE(t.Test):
    test_type = t.Short

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        ctx.exec('pmemset_source', self.test_case, filepath)


@g.no_testdir()
class PMEMSET_SOURCE_NO_DIR(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmemset_source', self.test_case)


class TEST0(PMEMSET_SOURCE):
    """allocation of pmemset_source in case of missing memory in system"""
    test_case = "test_alloc_src_enomem"


class TEST1(PMEMSET_SOURCE_NO_DIR):
    """testing pmemset_from_pmem2 with null value"""
    test_case = "test_set_from_pmem2_null"


class TEST2(PMEMSET_SOURCE):
    """valid allocation of pmemset_source from pmem2"""
    test_case = "test_set_from_pmem2_valid"


class TEST3(PMEMSET_SOURCE_NO_DIR):
    """test source creation from null file path"""
    test_case = "test_src_from_file_null"


class TEST4(PMEMSET_SOURCE):
    """test source creation with valid file path"""
    test_case = "test_src_from_file_valid"
