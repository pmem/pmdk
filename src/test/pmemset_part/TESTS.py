#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
class PMEMSET_PART(t.Test):
    test_type = t.Short
    create_file = True

    def run(self, ctx):
        filepath = "not/existing/file"
        if self.create_file:
            filepath = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        ctx.exec('pmemset_part', self.test_case, filepath)


class TEST0(PMEMSET_PART):
    """test pmemset_part allocation with error injection"""
    test_case = "test_part_new_enomem"


class TEST1(PMEMSET_PART):
    """create a new part from a source with invalid path assigned"""
    test_case = "test_part_new_invalid_source_file"
    create_file = False


class TEST2(PMEMSET_PART):
    """create a new part from a source with valid path assigned"""
    test_case = "test_part_new_valid_source_file"


class TEST3(PMEMSET_PART):
    """create a new part from a source with valid pmem2_source assigned"""
    test_case = "test_part_new_valid_source_pmem2"


class TEST4(PMEMSET_PART):
    """create a new part from a source with valid pmem2_source and map it"""
    test_case = "test_part_map_valid_source_pmem2"


class TEST5(PMEMSET_PART):
    """create a new part from a source with valid file path and map it"""
    test_case = "test_part_map_valid_source_file"


class TEST6(PMEMSET_PART):
    """test pmemset_part_map with invalid offset"""
    test_case = "test_part_map_invalid_offset"


@t.windows_exclude
@t.require_valgrind_enabled('memcheck')
class TEST7(PMEMSET_PART):
    """create a new part from a source with valid file path and map it"""
    test_case = "test_part_map_valid_source_file"


@t.windows_exclude
@t.require_valgrind_enabled('memcheck')
class TEST8(PMEMSET_PART):
    """create a new part from a source with valid pmem2 source and map it"""
    test_case = "test_part_map_valid_source_pmem2"


class TEST9(PMEMSET_PART):
    """test if data is unavailable after pmemset_delete"""
    test_case = "test_unmap_part"


class TEST10(PMEMSET_PART):
    """test pmemset_part_map allocation with error injection"""
    test_case = "test_part_map_enomem"


class TEST11(PMEMSET_PART):
    """get the first (earliest in the memory) mapping from the set"""
    test_case = "test_part_map_first"


class TEST12(PMEMSET_PART):
    """get the descriptor from the part map"""
    test_case = "test_part_map_descriptor"


class TEST13(PMEMSET_PART):
    """test retrieving next mapping from the set"""
    test_case = "test_part_map_next"


class TEST14(PMEMSET_PART):
    """test pmemset_get_store_granularity function"""
    test_case = "test_part_map_gran_read"


class TEST15(PMEMSET_PART):
    """test pmemset_part_map_drop function"""
    test_case = "test_part_map_drop"


class TEST16(PMEMSET_PART):
    """test reading part mapping by address"""
    test_case = "test_part_map_by_addr"
