#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2023, Intel Corporation
#


import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
class Pmem2_from_existing(t.Test):
    test_type = t.Short

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        ctx.exec('pmem2_map_from_existing', self.test_case, filepath)


class TEST0(Pmem2_from_existing):
    """try to create two the same mappings"""
    test_case = "test_two_same_mappings"


class TEST1(Pmem2_from_existing):
    """try to map which overlap bottom part of existing mapping"""
    test_case = "test_mapping_overlap_bottom"


class TEST2(Pmem2_from_existing):
    """try to map which overlap upper part of existing mapping"""
    test_case = "test_mapping_overlap_upper"


class TEST3(Pmem2_from_existing):
    """inject enomem in to allocation of map object"""
    test_case = "test_map_allocation_enomem"


class TEST4(Pmem2_from_existing):
    """inject enomem during adding map to ravl"""
    test_case = "test_register_mapping_enomem"
