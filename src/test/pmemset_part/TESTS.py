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


class PMEMSET_PART_ASYNC(t.Test):
    test_type = t.Short
    file_size = 16 * t.MiB

    def run(self, ctx):
        filepath = ctx.create_holey_file(self.file_size, 'testfile2',)
        ctx.exec('pmemset_part', self.test_case, filepath, self.threads,
                 self.ops_per_thread)


@t.linux_only
@t.require_devdax(t.DevDax('devdax1'))
class PMEMSET_PART_DEVDAX(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ddpath = ctx.devdaxes.devdax1.path
        ctx.exec('pmemset_part', self.test_case, ddpath)


class TEST0(PMEMSET_PART):
    """create a new part from a source with valid pmem2_source and map it"""
    test_case = "test_part_map_valid_source_pmem2"


class TEST1(PMEMSET_PART):
    """create a new part from a source with valid file path and map it"""
    test_case = "test_part_map_valid_source_file"


@t.windows_exclude
@t.require_valgrind_enabled('memcheck')
class TEST2(PMEMSET_PART):
    """create a new part from a source with valid file path and map it"""
    test_case = "test_part_map_valid_source_file"


@t.windows_exclude
@t.require_valgrind_enabled('memcheck')
class TEST3(PMEMSET_PART):
    """create a new part from a source with valid pmem2 source and map it"""
    test_case = "test_part_map_valid_source_pmem2"


class TEST4(PMEMSET_PART):
    """test if data is unavailable after pmemset_delete"""
    test_case = "test_unmap_part"


class TEST5(PMEMSET_PART):
    """test pmemset_part_map allocation with error injection"""
    test_case = "test_part_map_enomem"


class TEST6(PMEMSET_PART):
    """get the first (earliest in the memory) mapping from the set"""
    test_case = "test_part_map_first"


class TEST7(PMEMSET_PART):
    """get the descriptor from the part map"""
    test_case = "test_part_map_descriptor"


class TEST8(PMEMSET_PART):
    """test retrieving next mapping from the set"""
    test_case = "test_part_map_next"


class TEST9(PMEMSET_PART):
    """test pmemset_get_store_granularity function"""
    test_case = "test_part_map_gran_read"


class TEST10(PMEMSET_PART):
    """test pmemset_part_map_drop function"""
    test_case = "test_part_map_drop"


class TEST11(PMEMSET_PART):
    """test reading part mapping by address"""
    test_case = "test_part_map_by_addr"


class TEST12(PMEMSET_PART):
    """test creating a new part from file with unaligned size"""
    test_case = "test_part_map_unaligned_size"
    file_size = 16 * t.MiB - 1


class TEST13(PMEMSET_PART):
    """turn on full coalescing feature then create two mappings"""
    test_case = "test_part_map_full_coalesce_before"


class TEST14(PMEMSET_PART):
    """
    map a part, turn on the full coalescing feature and map a part second time
    """
    test_case = "test_part_map_full_coalesce_after"


class TEST15(PMEMSET_PART):
    """turn on opportunistic coalescing feature then create two mappings"""
    test_case = "test_part_map_opp_coalesce_before"


class TEST16(PMEMSET_PART):
    """
    map a part, turn on the opportunistic coalescing feature and
    map a part second time
    """
    test_case = "test_part_map_opp_coalesce_after"


class TEST17(PMEMSET_PART):
    """
    map two parts to the pmemset, iterate over
    the set to find them, then remove them
    """
    test_case = "test_remove_part_map"


class TEST18(PMEMSET_PART):
    """
    Enable the part coalescing feature, map two parts to the pmemset.
    If no error returned those parts should appear as single part mapping.
    Iterate over the set to obtain coalesced part mapping and delete it.
    """
    test_case = "test_full_coalescing_before_remove_part_map"


class TEST19(PMEMSET_PART):
    """
    Map two parts to the pmemset, iterate over the set to find first mapping
    and delete it. Turn on part coalescing and map new part. Lastly iterate
    over the set to find the coalesced mapping and delete it.
    """
    test_case = "test_full_coalescing_after_remove_first_part_map"


class TEST20(PMEMSET_PART):
    """
    Map two parts to the pmemset, iterate over the set to find second mapping
    and delete it. Turn on part coalescing and map new part. Lastly iterate
    over the set to find the coalesced mapping and delete it.
    """
    test_case = "test_full_coalescing_after_remove_second_part_map"


class TEST21(PMEMSET_PART):
    """
    Map hundred parts to the pmemset, iterate over the set to find the middle
    part mapping and delete it, repeat the process until there is not mappings
    left.
    """
    test_case = "test_remove_multiple_part_maps"


class TEST22(PMEMSET_PART):
    """create a new part from a temp source and map it"""
    test_case = "test_part_map_valid_source_temp"
    file_temp = True
    create_file = False


class TEST23(PMEMSET_PART):
    """create a new part from a temp source and map it with invalid size"""
    test_case = "test_part_map_invalid_source_temp"
    file_temp = True
    create_file = False


class TEST24(PMEMSET_PART):
    """
    Create a new part from a file source with do_not_grow flag set
    and map it.
    """
    test_case = "test_part_map_source_do_not_grow"
    path_only = True
    create_file = False


class TEST25(PMEMSET_PART):
    """
    Create a new part from a file source with do_not_grow flag set
    and pmemset_map_config_length unset and map it.
    """
    test_case = "test_part_map_source_do_not_grow_len_unset"
    path_only = True
    create_file = False


class TEST26(PMEMSET_PART):
    """
    Create a new part from a file source with do_not_grow flag unset
    and pmemset_map_config_length unset and map it.
    """
    test_case = "test_part_map_source_len_unset"
    path_only = True
    create_file = False


class TEST27(PMEMSET_PART):
    """
    Map two parts to the pmemset, iterate over the set to get mappings'
    regions. Select a region that encrouches on both of those mappings with
    minimum size and delete them.
    """
    test_case = "test_remove_two_ranges"


class TEST28(PMEMSET_PART):
    """
    Create two coalesced mappings, each composed of two parts, iterate over the
    set to get mappings' regions. Select a region that encrouches on both of
    those coalesced mappings containing one part each and delete them.
    """
    test_case = "test_remove_coalesced_two_ranges"


class TEST29(PMEMSET_PART):
    """
    Create coalesced mapping composed of three parts, iterate over the set
    to get mapping's region. Select a region that encrouches only on the part
    situated in the middle of the coalesced part mapping and delete it.
    """
    test_case = "test_remove_coalesced_middle_range"


class TEST30(PMEMSET_PART_ASYNC):
    """asynchronously map new and remove multiple parts to the pmemset"""
    test_case = "test_pmemset_async_map_remove_multiple_part_maps"
    threads = 32
    ops_per_thread = 1000


class TEST31(PMEMSET_PART):
    """
    create coalesced mapping composed of five parts, remove pmemset range two
    times to divide initial mapping into three mappings, remove all three
    mappings
    """
    test_case = "test_divide_coalesced_remove_obtained_pmaps"


class TEST32(PMEMSET_PART):
    """
    create a pmem2 vm reservation with the size of three files and set it in
    the pmemset config, map three files to the pmemset
    """
    test_case = "test_part_map_with_set_reservation"


class TEST33(PMEMSET_PART):
    """
    create a pmem2 vm reservation with the size of three files and set it in
    the pmemset config, turn on full coalescing and map three files to the
    pmemset
    """
    test_case = "test_part_map_coalesce_with_set_reservation"


class TEST34(PMEMSET_PART):
    """
    create a pmem2 vm reservation with half the size of one file and set it in
    the pmemset config, try to map a part to the pmemset
    """
    test_case = "test_part_map_with_set_reservation_too_small"


class TEST35(PMEMSET_PART):
    """
    create a pmem2 vm reservation with the size of three files and set it in
    the pmemset config, map six parts with half the size of one file, remove
    every second part mapping then try to map a part with the size of one file
    """
    test_case = "test_part_map_with_set_reservation_cannot_fit"


class TEST36(PMEMSET_PART_DEVDAX):
    """
    devdax create a new part from a source with valid pmem2_source and map it
    """
    test_case = "test_part_map_valid_source_pmem2"


class TEST37(PMEMSET_PART_DEVDAX):
    """
    create a new part from a source with valid file path and map it
    """
    test_case = "test_part_map_valid_source_file"


class TEST38(PMEMSET_PART_DEVDAX):
    """devdax get the first (earliest in the memory) mapping from the set"""
    test_case = "test_part_map_first"


class TEST39(PMEMSET_PART_DEVDAX):
    """devdax get the descriptor from the part map"""
    test_case = "test_part_map_descriptor"


class TEST40(PMEMSET_PART_DEVDAX):
    """devdax test retrieving next mapping from the set"""
    test_case = "test_part_map_next"


class TEST41(PMEMSET_PART_DEVDAX):
    """devdax test reading part mapping by address"""
    test_case = "test_part_map_by_addr"


class TEST42(PMEMSET_PART_DEVDAX):
    """devdax turn on full coalescing feature then create two mappings"""
    test_case = "test_part_map_full_coalesce_before"


class TEST43(PMEMSET_PART_DEVDAX):
    """
    devdax map a part, turn on the full coalescing feature and map a part
    second time
    """
    test_case = "test_part_map_full_coalesce_after"


class TEST44(PMEMSET_PART_DEVDAX):
    """
    devdax turn on opportunistic coalescing feature then create two mappings
    """
    test_case = "test_part_map_opp_coalesce_before"


class TEST45(PMEMSET_PART_DEVDAX):
    """
    devdax map a part, turn on the opportunistic coalescing feature and
    map a part second time
    """
    test_case = "test_part_map_opp_coalesce_after"
