#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2021, Intel Corporation
#

import os
import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
class PMEMSET_PART(t.Test):
    test_type = t.Short
    create_file = True
    file_size = 16 * t.MiB
    file_temp = False
    path_only = False

    def run(self, ctx):
        filepath = "not/existing/file"
        if self.create_file:
            filepath = ctx.create_holey_file(self.file_size, 'testfile1')
        if self.path_only:
            filepath = os.path.join(ctx.testdir, 'testfile1')
        if self.file_temp:
            filepath = ctx.testdir
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


class TEST17(PMEMSET_PART):
    """test creating a new part from file with unaligned size"""
    test_case = "test_part_map_unaligned_size"
    file_size = 16 * t.MiB - 1


class TEST18(PMEMSET_PART):
    """turn on full coalescing feature then create two mappings"""
    test_case = "test_part_map_full_coalesce_before"
    file_size = 64 * t.KiB


class TEST19(PMEMSET_PART):
    """
    map a part, turn on the full coalescing feature and map a part second time
    """
    test_case = "test_part_map_full_coalesce_after"
    file_size = 64 * t.KiB


class TEST20(PMEMSET_PART):
    """turn on opportunistic coalescing feature then create two mappings"""
    test_case = "test_part_map_opp_coalesce_before"
    file_size = 64 * t.KiB


class TEST21(PMEMSET_PART):
    """
    map a part, turn on the opportunistic coalescing feature and
    map a part second time
    """
    test_case = "test_part_map_full_coalesce_after"
    file_size = 64 * t.KiB


class TEST22(PMEMSET_PART):
    """
    map two parts to the pmemset, iterate over
    the set to find them, then remove them
    """
    test_case = "test_remove_part_map"


class TEST23(PMEMSET_PART):
    """
    Enable the part coalescing feature, map two parts to the pmemset.
    If no error returned those parts should appear as single part mapping.
    Iterate over the set to obtain coalesced part mapping and delete it.
    """
    test_case = "test_full_coalescing_before_remove_part_map"


class TEST24(PMEMSET_PART):
    """
    Map two parts to the pmemset, iterate over the set to find first mapping
    and delete it. Turn on part coalescing and map new part. Lastly iterate
    over the set to find the coalesced mapping and delete it.
    """
    test_case = "test_full_coalescing_after_remove_first_part_map"


class TEST25(PMEMSET_PART):
    """
    Map two parts to the pmemset, iterate over the set to find second mapping
    and delete it. Turn on part coalescing and map new part. Lastly iterate
    over the set to find the coalesced mapping and delete it.
    """
    test_case = "test_full_coalescing_after_remove_second_part_map"


class TEST26(PMEMSET_PART):
    """
    Map hundred parts to the pmemset, iterate over the set to find the middle
    part mapping and delete it, repeat the process until there is not mappings
    left.
    """
    test_case = "test_remove_multiple_part_maps"


class TEST27(PMEMSET_PART):
    """create a new part from a temp source and map it"""
    test_case = "test_part_map_valid_source_temp"
    file_temp = True
    create_file = False


class TEST28(PMEMSET_PART):
    """create a new part from a temp source and map it with invalid size"""
    test_case = "test_part_map_invalid_source_temp"
    file_temp = True
    create_file = False


class TEST29(PMEMSET_PART):
    """create a new part from a file source with truncate flag and map it"""
    test_case = "test_part_map_source_truncate"
    path_only = True
    create_file = False
