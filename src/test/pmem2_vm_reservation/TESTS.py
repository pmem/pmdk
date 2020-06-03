#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
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


class PMEM2_VM_RESERVATION_NO_FILE(t.Test):
    test_type = t.Short
    reserv_size = 16 * t.MiB
    with_reserv_size = True

    def run(self, ctx):
        if self.with_reserv_size:
            ctx.exec('pmem2_vm_reservation', self.test_case, self.reserv_size)
        else:
            ctx.exec('pmem2_vm_reservation', self.test_case)


class PMEM2_VM_RESERVATION_ASYNC(t.Test):
    test_type = t.Short
    filesize = 16 * t.MiB

    def run(self, ctx):
        filepath = ctx.create_holey_file(self.filesize, 'testfile',)
        filesize = os.stat(filepath).st_size
        ctx.exec('pmem2_vm_reservation', self.test_case, filepath,
                 filesize, self.threads, self.ops_per_thread)


class TEST0(PMEM2_VM_RESERVATION):
    """create a vm reservation in the region belonging to existing mapping"""
    test_case = "test_vm_reserv_new_region_occupied_map"


class TEST1(PMEM2_VM_RESERVATION_NO_FILE):
    """
    create a vm reservation in the region belonging to other
    existing vm reservation
    """
    test_case = "test_vm_reserv_new_region_occupied_reserv"


class TEST2(PMEM2_VM_RESERVATION):
    """delete a vm reservation that contains a mapping in the middle"""
    test_case = "test_vm_reserv_delete_contains_mapping"


class TEST3(PMEM2_VM_RESERVATION):
    """map a file to a vm reservation"""
    test_case = "test_vm_reserv_map_file"


class TEST4(PMEM2_VM_RESERVATION):
    """
    map a half of the file to a vm reservation smaller than the whole file
    """
    test_case = "test_vm_reserv_map_half_file"


class TEST5(PMEM2_VM_RESERVATION):
    """
    map multiple files to a vm reservation, then
    unmap every 2nd mapping and map the mappings again
    """
    test_case = "test_vm_reserv_map_unmap_multiple_files"


class TEST6(PMEM2_VM_RESERVATION):
    """map a file to a vm reservation with insufficient space"""
    test_case = "test_vm_reserv_map_insufficient_space"


class TEST7(PMEM2_VM_RESERVATION):
    """
    map a file to a vm reservation and overlap whole other existing mapping
    belonging to the same reservation
    """
    test_case = "test_vm_reserv_map_full_overlap"


class TEST8(PMEM2_VM_RESERVATION):
    """
    map a file to a vm reservation overlapping with the ealier half
    of the other existing mapping
    """
    test_case = "test_vm_reserv_map_partial_overlap_below"


class TEST9(PMEM2_VM_RESERVATION):
    """
    map a file to a vm reservation overlapping with the latter half
    of the other existing mapping
    """
    test_case = "test_vm_reserv_map_partial_overlap_above"


@g.require_granularity(g.PAGE, g.CACHELINE)
class TEST10(PMEM2_VM_RESERVATION):
    """
    map a file with invalid granularity to a vm reservation in the middle of
    the vm reservation bigger than the file, then map a file that covers whole
    vm reservation
    """
    test_case = "test_vm_reserv_map_invalid_granularity"


class TEST11(PMEM2_VM_RESERVATION_ASYNC):
    """
    map and unmap asynchronously multiple times to the whole vm reservation
    region
    """
    test_case = "test_vm_reserv_async_map_unmap_multiple_files"
    threads = 32
    ops_per_thread = 10000
