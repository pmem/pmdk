#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
class PMEMSET_FILE(t.Test):
    test_type = t.Short
    create_file = True

    def run(self, ctx):
        filepath = "not/existing/file"
        if self.create_file:
            filepath = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        ctx.exec('pmemset_file', self.test_case, filepath)


@g.no_testdir()
class PMEMSET_FILE_NO_DIR(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmemset_file', self.test_case)


class TEST0(PMEMSET_FILE):
    """test pmemset_file allocation with error injection"""
    test_case = "test_alloc_file_enomem"


class TEST1(PMEMSET_FILE):
    """test valid pmemset_file allocation"""
    test_case = "test_file_from_file_valid"


class TEST2(PMEMSET_FILE):
    """test pmemset_file allocation from invalid path"""
    test_case = "test_file_from_file_invalid"
    create_file = False


class TEST3(PMEMSET_FILE):
    """test valid pmemset_file allocation from pmem2"""
    test_case = "test_file_from_pmem2_valid"


class TEST4(PMEMSET_FILE_NO_DIR):
    """test pmemset_file allocation from invalid pmem2_source"""
    test_case = "test_file_from_pmem2_invalid"


class TEST5(PMEMSET_FILE):
    """
    test retrieving pmem2_src stored in the pmemset_file created from file
    """
    test_case = "test_file_from_file_get_pmem2_src"


class TEST6(PMEMSET_FILE):
    """
    test retrieving pmem2_source stored in the pmemset_file created
    from pmem2_source
    """
    test_case = "test_file_from_pmem2_get_pmem2_src"
