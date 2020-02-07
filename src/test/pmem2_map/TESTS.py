#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#

import os

import testframework as t


class PMEM2_MAP(t.Test):
    test_type = t.Short
    filesize = 16 * t.MiB
    with_size = True

    def run(self, ctx):
        filepath = ctx.create_holey_file(self.filesize, 'testfile',)
        if self.with_size:
            filesize = os.stat(filepath).st_size
            ctx.exec('pmem2_map', self.test_case, filepath, filesize)
        else:
            ctx.exec('pmem2_map', self.test_case, filepath)


class PMEM2_MAP_NO_FILE(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_map', self.test_case)


@t.windows_exclude
@t.require_devdax(t.DevDax('devdax1'))
class PMEM2_MAP_DEVDAX(t.Test):
    test_type = t.Short
    with_size = True

    def run(self, ctx):
        dd = ctx.devdaxes.devdax1
        if self.with_size:
            ctx.exec('pmem2_map', self.test_case, dd.path, dd.size)
        else:
            ctx.exec('pmem2_map', self.test_case, dd.path)


class TEST0(PMEM2_MAP):
    """map a O_RDWR file"""
    test_case = "test_map_rdrw_file"
    with_size = False


class TEST1(PMEM2_MAP_DEVDAX):
    """DevDax map a O_RDWR file"""
    test_case = "test_map_rdrw_file"
    with_size = False


class TEST2(PMEM2_MAP):
    """map a O_RDONLY file"""
    test_case = "test_map_rdonly_file"
    with_size = False


class TEST3(PMEM2_MAP_DEVDAX):
    """DevDax map a O_RDONLY file"""
    test_case = "test_map_rdonly_file"
    with_size = False


class TEST4(PMEM2_MAP):
    """map a O_WRONLY file"""
    test_case = "test_map_wronly_file"
    with_size = False


class TEST5(PMEM2_MAP_DEVDAX):
    """DevDax map a O_WRONLY file"""
    test_case = "test_map_wronly_file"
    with_size = False


class TEST6(PMEM2_MAP):
    """map valid memory ranges"""
    test_case = "test_map_valid_ranges"


class TEST7(PMEM2_MAP_DEVDAX):
    """DevDax map valid memory ranges"""
    test_case = "test_map_valid_ranges"


class TEST8(PMEM2_MAP):
    """map invalid memory ranges"""
    test_case = "test_map_invalid_ranges"


class TEST9(PMEM2_MAP_DEVDAX):
    """DevDax map invalid memory ranges"""
    test_case = "test_map_invalid_ranges"


class TEST10(PMEM2_MAP):
    """map using invalid alignment in the offset"""
    test_case = "test_map_invalid_alignment"


class TEST11(PMEM2_MAP_DEVDAX):
    """DevDax map using invalid alignment in the offset"""
    test_case = "test_map_invalid_alignment"


class TEST12(PMEM2_MAP):
    """map using a invalid file descriptor"""
    test_case = "test_map_invalid_fd"


class TEST13(PMEM2_MAP):
    """unmap valid pmem2 mapping"""
    test_case = "test_unmap_valid"


class TEST14(PMEM2_MAP_DEVDAX):
    """DevDax unmap valid pmem2 mapping"""
    test_case = "test_unmap_valid"


# UnmapViewOfFile does not use length
@t.windows_exclude
class TEST15(PMEM2_MAP):
    """unmap a pmem2 mapping with an invalid length"""
    test_case = "test_unmap_zero_length"


class TEST16(PMEM2_MAP_DEVDAX):
    """DevDax unmap a pmem2 mapping with an invalid length"""
    test_case = "test_unmap_zero_length"


# UnmapViewOfFile does not care about the address alignment
@t.windows_exclude
class TEST17(PMEM2_MAP):
    """unmap a pmem2 mapping with an unaligned address"""
    test_case = "test_unmap_unaligned_addr"


class TEST18(PMEM2_MAP_DEVDAX):
    """DevDax unmap a pmem2 mapping with an unaligned address"""
    test_case = "test_unmap_unaligned_addr"


# munmap does not fail if the mapping does not exist
@t.windows_only
class TEST19(PMEM2_MAP):
    """double unmap a pmem2 mapping"""
    test_case = "test_unmap_unmapped"


class TEST20(PMEM2_MAP_NO_FILE):
    """test for pmem2_map_get_address"""
    test_case = "test_map_get_address"


class TEST21(PMEM2_MAP_NO_FILE):
    """test for pmem2_map_get_size"""
    test_case = "test_map_get_size"


class TEST22(PMEM2_MAP_NO_FILE):
    """simply get the previously stored value of granularity"""
    test_case = "test_get_granularity_simple"


class TEST23(PMEM2_MAP):
    """map a file of length which is not page-aligned"""
    test_case = "test_map_unaligned_length"
    filesize = 3 * t.KiB


class TEST24(PMEM2_MAP):
    """map a file which size is not aligned"""
    test_case = "test_map_larger_than_unaligned_file_size"
    filesize = 16 * t.MiB - 1


class TEST25(PMEM2_MAP):
    """
    map a file with zero size, do not provide length
    to pmem2_map config
    """
    test_case = "test_map_zero_file_size"
    filesize = 0


class TEST26(PMEM2_MAP):
    """
    map a file with PMEM2_SHARED sharing, changes in the mapping are visible
    in another mapping
    """
    test_case = "test_map_sharing_shared"
    with_size = False


class TEST27(PMEM2_MAP):
    """
    map a file with PMEM2_PRIVATE sharing, changes in the mapping are not
    visible in another mapping
    """
    test_case = "test_map_sharing_private"
    with_size = False


class TEST28(PMEM2_MAP):
    """
    map a file with PMEM2_PRIVATE sharing, changes in the mapping are not
    visible in another mapping, fd is reopened before each mapping
    """
    test_case = "test_map_sharing_private_with_reopened_fd"
    with_size = False


class TEST29(PMEM2_MAP):
    """
    map O_RDONLY file with PMEM2_PRIVATE sharing
    """
    test_case = "test_map_sharing_private_rdonly_file"
    with_size = False


class TEST30(PMEM2_MAP_DEVDAX):
    """DevDax file with PMEM2_PRIVATE sharing"""
    test_case = "test_map_sharing_private_devdax"
    with_size = False


# XXX: remove when PMEM2_ADDRESS_FIXED_NOREPLACE will be supported on Windows
@t.windows_exclude
class TEST31(PMEM2_MAP):
    """
    map a file to the desired addr with request type
    PMEM2_ADDRESS_FIXED_NOREPLACE
    """
    test_case = "test_map_fixed_noreplace_valid"


# XXX: remove when PMEM2_ADDRESS_FIXED_NOREPLACE will be supported on Windows
@t.windows_exclude
class TEST32(PMEM2_MAP):
    """
    map a file and overlap whole other existing mapping with the request type
    PMEM2_ADDRESS_FIXED_NOREPLACE
    """
    test_case = "test_map_fixed_noreplace_full_overlap"


# XXX: remove when PMEM2_ADDRESS_FIXED_NOREPLACE will be supported on Windows
@t.windows_exclude
class TEST33(PMEM2_MAP):
    """
    map a file in a middle of other existing mapping with the request type
    PMEM2_ADDRESS_FIXED_NOREPLACE
    """
    test_case = "test_map_fixed_noreplace_partial_overlap"


# XXX: remove when PMEM2_ADDRESS_FIXED_NOREPLACE will be supported on Windows
@t.windows_exclude
class TEST34(PMEM2_MAP):
    """
    map a file which starts in a middle and ends above of other
    existing mapping with request type PMEM2_ADDRESS_FIXED_NOREPLACE
    """
    test_case = "test_map_fixed_noreplace_partial_above_overlap"
