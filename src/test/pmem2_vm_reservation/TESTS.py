#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2023, Intel Corporation
#

import os

import testframework as t
from testframework import granularity as g


class PMEM2_VM_RESERVATION(t.Test):
    test_type = t.Short
    filesize = 16 * t.MiB
    with_size = True

    def run(self, ctx):
        filepath = ctx.create_holey_file(self.filesize, 'testfile',)
        if self.with_size:
            filesize = os.stat(filepath).st_size
            ctx.exec('pmem2_vm_reservation', self.test_case, filepath,
                     filesize)
        else:
            ctx.exec('pmem2_vm_reservation', self.test_case, filepath)


class PMEM2_VM_RESERVATION_ASYNC(t.Test):
    test_type = t.Short
    filesize = 16 * t.MiB

    def run(self, ctx):
        filepath = ctx.create_holey_file(self.filesize, 'testfile',)
        filesize = os.stat(filepath).st_size
        ctx.exec('pmem2_vm_reservation', self.test_case, filepath,
                 filesize, self.threads, self.ops_per_thread)


@t.windows_exclude
@t.require_devdax(t.DevDax('devdax1'))
class PMEM2_VM_RESERVATION_DEVDAX(t.Test):
    test_type = t.Short
    with_size = True

    def run(self, ctx):
        dd = ctx.devdaxes.devdax1
        if self.with_size:
            ctx.exec('pmem2_vm_reservation', self.test_case, dd.path, dd.size)
        else:
            ctx.exec('pmem2_vm_reservation', self.test_case, dd.path)


@t.windows_exclude
@t.require_devdax(t.DevDax('devdax1'))
class PMEM2_VM_RESERVATION_ASYNC_DEVDAX(t.Test):
    test_type = t.Short
    with_size = True

    def run(self, ctx):
        dd = ctx.devdaxes.devdax1
        if self.with_size:
            ctx.exec('pmem2_vm_reservation', self.test_case, dd.path, dd.size,
                     self.threads, self.ops_per_thread)
        else:
            ctx.exec('pmem2_vm_reservation', self.test_case, dd.path,
                     self.threads, self.ops_per_thread)


class TEST0(PMEM2_VM_RESERVATION):
    """create a vm reservation with unaligned address provided"""
    test_case = "test_vm_reserv_new_unaligned_addr"


class TEST1(PMEM2_VM_RESERVATION_DEVDAX):
    """DevDax create a vm reservation with unaligned address provided"""
    test_case = "test_vm_reserv_new_unaligned_addr"


class TEST2(PMEM2_VM_RESERVATION):
    """create a vm reservation with unaligned size provided"""
    test_case = "test_vm_reserv_new_unaligned_size"


class TEST3(PMEM2_VM_RESERVATION_DEVDAX):
    """DevDax create a vm reservation with unaligned size provided"""
    test_case = "test_vm_reserv_new_unaligned_size"


class TEST4(PMEM2_VM_RESERVATION):
    """
    map a file to the desired addr  with the help of virtual
    memory reservation
    """
    test_case = "test_vm_reserv_new_valid_addr"


class TEST5(PMEM2_VM_RESERVATION_DEVDAX):
    """
    DevDax map a file to the desired addr  with the help of virtual
    memory reservation
    """
    test_case = "test_vm_reserv_new_valid_addr"


class TEST6(PMEM2_VM_RESERVATION):
    """
    create a vm reservation in the region belonging to other
    existing vm reservation
    """
    test_case = "test_vm_reserv_new_region_occupied_reserv"


class TEST7(PMEM2_VM_RESERVATION_DEVDAX):
    """
    DevDax create a vm reservation in the region belonging to other
    existing vm reservation
    """
    test_case = "test_vm_reserv_new_region_occupied_reserv"


class TEST8(PMEM2_VM_RESERVATION):
    """
    create a vm reservation in the region overlapping whole existing mapping
    """
    test_case = "test_vm_reserv_new_region_occupied_map"


class TEST9(PMEM2_VM_RESERVATION_DEVDAX):
    """
    DevDax create a vm reservation in the region overlapping
    whole existing mapping
    """
    test_case = "test_vm_reserv_new_region_occupied_map"


class TEST10(PMEM2_VM_RESERVATION):
    """
    create a reservation in the region overlapping lower half of
    the existing mapping
    """
    test_case = "test_vm_reserv_new_region_occupied_map_below"


# XXX disable the test for 'pmemcheck', 'drd', 'helgrind', 'memcheck'
# until https://github.com/pmem/pmdk/issues/5592 is fixed.
@t.require_valgrind_disabled('pmemcheck', 'drd', 'helgrind', 'memcheck')
class TEST11(PMEM2_VM_RESERVATION_DEVDAX):
    """
    DevDax create a reservation in the region overlapping lower half of
    the existing mapping
    """
    test_case = "test_vm_reserv_new_region_occupied_map_below"


class TEST12(PMEM2_VM_RESERVATION):
    """
    create a reservation in the region overlapping upper half of
    the existing mapping
    """
    test_case = "test_vm_reserv_new_region_occupied_map_above"


class TEST13(PMEM2_VM_RESERVATION_DEVDAX):
    """
    DevDax create a reservation in the region overlapping upper half of
    the existing mapping
    """
    test_case = "test_vm_reserv_new_region_occupied_map_above"


class TEST14(PMEM2_VM_RESERVATION):
    """create a vm reservation with with error injection"""
    test_case = "test_vm_reserv_new_alloc_enomem"


class TEST15(PMEM2_VM_RESERVATION_DEVDAX):
    """DevDax create a vm reservation with with error injection"""
    test_case = "test_vm_reserv_new_alloc_enomem"


class TEST16(PMEM2_VM_RESERVATION):
    """map a file to a vm reservation"""
    test_case = "test_vm_reserv_map_file"


class TEST17(PMEM2_VM_RESERVATION_DEVDAX):
    """DevDax map a file to a vm reservation"""
    test_case = "test_vm_reserv_map_file"


class TEST18(PMEM2_VM_RESERVATION):
    """
    map a part of the file to a vm reservation smaller than the whole file
    """
    test_case = "test_vm_reserv_map_part_file"


class TEST19(PMEM2_VM_RESERVATION_DEVDAX):
    """
    DevDax map a part of the file to a vm reservation
    smaller than the whole file
    """
    test_case = "test_vm_reserv_map_part_file"


class TEST20(PMEM2_VM_RESERVATION):
    """delete a vm reservation that contains a mapping"""
    test_case = "test_vm_reserv_delete_contains_mapping"


class TEST21(PMEM2_VM_RESERVATION):
    """
    map multiple files to a vm reservation, then
    unmap every 2nd mapping and map the mappings again
    """
    test_case = "test_vm_reserv_map_unmap_multiple_files"


class TEST22(PMEM2_VM_RESERVATION_DEVDAX):
    """
    DevDax map multiple files to a vm reservation, then
    unmap every 2nd mapping and map the mappings again
    """
    test_case = "test_vm_reserv_map_unmap_multiple_files"


class TEST23(PMEM2_VM_RESERVATION):
    """map a file to a vm reservation with insufficient space"""
    test_case = "test_vm_reserv_map_insufficient_space"


class TEST24(PMEM2_VM_RESERVATION):
    """
    map a file to a vm reservation and overlap whole other existing mapping
    belonging to the same reservation
    """
    test_case = "test_vm_reserv_map_full_overlap"


class TEST25(PMEM2_VM_RESERVATION_DEVDAX):
    """
    DevDax map a file to a vm reservation and overlap whole other
    existing mapping belonging to the same reservation
    """
    test_case = "test_vm_reserv_map_full_overlap"


class TEST26(PMEM2_VM_RESERVATION):
    """
    map a file to a vm reservation overlapping with the earlier half
    of the other existing mapping
    """
    test_case = "test_vm_reserv_map_partial_overlap_below"


class TEST27(PMEM2_VM_RESERVATION_DEVDAX):
    """
    DevDax map a file to a vm reservation overlapping with the earlier half
    of the other existing mapping
    """
    test_case = "test_vm_reserv_map_partial_overlap_below"


class TEST28(PMEM2_VM_RESERVATION):
    """
    map a file to a vm reservation overlapping with the latter half
    of the other existing mapping
    """
    test_case = "test_vm_reserv_map_partial_overlap_above"


class TEST29(PMEM2_VM_RESERVATION_DEVDAX):
    """
    DevDax map a file to a vm reservation overlapping with the latter half
    of the other existing mapping
    """
    test_case = "test_vm_reserv_map_partial_overlap_above"


@g.require_granularity(g.PAGE, g.CACHELINE)
class TEST30(PMEM2_VM_RESERVATION):
    """
    map a file with invalid granularity to a vm reservation in the middle of
    the vm reservation bigger than the file, then map a file that covers whole
    vm reservation
    """
    test_case = "test_vm_reserv_map_invalid_granularity"


class TEST31(PMEM2_VM_RESERVATION_ASYNC):
    """
    map and unmap asynchronously multiple times to the whole vm reservation
    region
    """
    test_case = "test_vm_reserv_async_map_unmap_multiple_files"
    threads = 32
    ops_per_thread = 1000


# XXX disable the test for 'pmemcheck', 'drd', 'helgrind', 'memcheck'
# until https://github.com/pmem/pmdk/issues/5592 is fixed.
@t.require_valgrind_disabled('pmemcheck', 'drd', 'helgrind', 'memcheck')
class TEST32(PMEM2_VM_RESERVATION_ASYNC_DEVDAX):
    """
    DevDax map and unmap asynchronously multiple times to the whole
    whole vm reservation region
    """
    test_case = "test_vm_reserv_async_map_unmap_multiple_files"
    threads = 32
    ops_per_thread = 1000


class TEST33(PMEM2_VM_RESERVATION):
    """extend the empty vm reservation"""
    test_case = "test_vm_reserv_empty_extend"


class TEST34(PMEM2_VM_RESERVATION):
    """map a file to a vm reservation, extend the reservation and map again"""
    test_case = "test_vm_reserv_map_extend"


class TEST35(PMEM2_VM_RESERVATION):
    """extend the empty vm reservation by unaligned size"""
    test_case = "test_vm_reserv_unaligned_extend"


class TEST36(PMEM2_VM_RESERVATION):
    """
    shrink the empty vm reservation from the start, then from the end,
    lastly map a file to it
    """
    test_case = "test_vm_reserv_empty_shrink"


class TEST37(PMEM2_VM_RESERVATION):
    """
    map a file to the reservation, shrink the reservation from the start,
    then from the end
    """
    test_case = "test_vm_reserv_map_shrink"


class TEST38(PMEM2_VM_RESERVATION):
    """
    shrink the empty vm reservation with unaligned offset,
    then with unaligned size
    """
    test_case = "test_vm_reserv_unaligned_shrink"


class TEST39(PMEM2_VM_RESERVATION):
    """
    shrink the empty vm reservation by interval (offset, offset + size) that is
    out of available range for the reservation to be shrunk
    """
    test_case = "test_vm_reserv_out_of_range_shrink"


class TEST40(PMEM2_VM_RESERVATION):
    """
    shrink the empty vm reservation from the middle, then try shrinking
    reservation by its whole range
    """
    test_case = "test_vm_reserv_unsupported_shrink"


class TEST41(PMEM2_VM_RESERVATION):
    """shrink the vm reservation by the region that is occupied"""
    test_case = "test_vm_reserv_occupied_region_shrink"


class TEST42(PMEM2_VM_RESERVATION):
    """
    create a reservation with exactly the size of a file and map a file to it,
    search for the mapping with the following intervals (offset, size):
    1. (reserv_start, reserv_middle), 2. (reserv_middle, reserv_end),
    3. (reserv_start, reserv_end)
    """
    test_case = "test_vm_reserv_one_map_find"


class TEST43(PMEM2_VM_RESERVATION):
    """
    create a reservation with exactly the size of a 2x file size and map
    a file to it two times, occupying the whole reservation, search for the
    mapping with the following intervals (offset, size):
    1. (reserv_start, reserv_middle), 2. (reserv_middle, reserv_end),
    3. (reserv_start, reserv_end)
    """
    test_case = "test_vm_reserv_two_maps_find"


class TEST44(PMEM2_VM_RESERVATION):
    """
    create a reservation with exactly the size of a 10x file size and map a
    file to it 5 times leaving equal space between each mapping, search the
    reservation for previous mapping for each mapping
    """
    test_case = "test_vm_reserv_prev_map_find"


class TEST45(PMEM2_VM_RESERVATION):
    """
    create a reservation with exactly the size of a 10x file size and map a
    file to it 5 times leaving equal space between each mapping, search the
    reservation for next mapping for each mapping
    """
    test_case = "test_vm_reserv_next_map_find"


class TEST46(PMEM2_VM_RESERVATION):
    """
    create a reservation with exactly the size of a 10x file size and map a
    file to it 5 times leaving equal space between each mapping, search the
    reservation for next mapping for each mapping
    """
    test_case = "test_vm_reserv_not_existing_prev_next_map_find"


class TEST47(PMEM2_VM_RESERVATION):
    """
    create a reservation with exactly the size of 1 file size and map a file to
    it, search for the first and last mapping in the reservation
    """
    test_case = "test_vm_reserv_same_first_last_map_find"


class TEST48(PMEM2_VM_RESERVATION):
    """
    create a reservation with exactly the size of 10 file size and map a file
    10 times to it, search for the first and last mapping in the reservation
    and delete them, repeat until no mappings are left
    """
    test_case = "test_vm_reserv_first_last_map_find"
