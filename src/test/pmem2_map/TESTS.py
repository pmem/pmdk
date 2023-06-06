#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2023, Intel Corporation
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


# XXX disable the test for `memcheck'
# until https://github.com/pmem/pmdk/issues/5600 is fixed.
@t.require_valgrind_disabled('memcheck')
class TEST0(PMEM2_MAP):
    """map a O_RDWR file"""
    test_case = "test_map_rdrw_file"
    with_size = False


# XXX disable the test for `memcheck'
# until https://github.com/pmem/pmdk/issues/5600 is fixed.
@t.require_valgrind_disabled('memcheck')
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


# XXX disable the test for `memcheck'
# until https://github.com/pmem/pmdk/issues/5600 is fixed.
@t.require_valgrind_disabled('memcheck')
class TEST4(PMEM2_MAP):
    """map valid memory ranges"""
    test_case = "test_map_valid_ranges"


# XXX disable the test for `memcheck'
# until https://github.com/pmem/pmdk/issues/5600 is fixed.
@t.require_valgrind_disabled('memcheck')
class TEST5(PMEM2_MAP_DEVDAX):
    """DevDax map valid memory ranges"""
    test_case = "test_map_valid_ranges"


class TEST6(PMEM2_MAP):
    """map invalid memory ranges"""
    test_case = "test_map_invalid_ranges"


class TEST7(PMEM2_MAP_DEVDAX):
    """DevDax map invalid memory ranges"""
    test_case = "test_map_invalid_ranges"


class TEST8(PMEM2_MAP):
    """map using invalid alignment in the offset"""
    test_case = "test_map_invalid_alignment"


class TEST9(PMEM2_MAP_DEVDAX):
    """DevDax map using invalid alignment in the offset"""
    test_case = "test_map_invalid_alignment"


class TEST10(PMEM2_MAP):
    """map using a invalid file descriptor"""
    test_case = "test_map_invalid_fd"


# XXX disable the test for `memcheck'
# until https://github.com/pmem/pmdk/issues/5600 is fixed.
@t.require_valgrind_disabled('memcheck')
class TEST11(PMEM2_MAP):
    """unmap valid pmem2 mapping"""
    test_case = "test_unmap_valid"


# XXX disable the test for `memcheck'
# until https://github.com/pmem/pmdk/issues/5600 is fixed.
@t.require_valgrind_disabled('memcheck')
class TEST12(PMEM2_MAP_DEVDAX):
    """DevDax unmap valid pmem2 mapping"""
    test_case = "test_unmap_valid"


# XXX disable the test for `memcheck'
# until https://github.com/pmem/pmdk/issues/5600 is fixed.
@t.require_valgrind_disabled('memcheck')
class TEST13(PMEM2_MAP):
    """unmap a pmem2 mapping with an invalid length"""
    test_case = "test_unmap_zero_length"


# XXX disable the test for `memcheck'
# until https://github.com/pmem/pmdk/issues/5600 is fixed.
@t.require_valgrind_disabled('memcheck')
class TEST14(PMEM2_MAP_DEVDAX):
    """DevDax unmap a pmem2 mapping with an invalid length"""
    test_case = "test_unmap_zero_length"


# UnmapViewOfFile does not care about the address alignment
# XXX disable the test for `memcheck'
# until https://github.com/pmem/pmdk/issues/5600 is fixed.
@t.require_valgrind_disabled('memcheck')
class TEST15(PMEM2_MAP):
    """unmap a pmem2 mapping with an unaligned address"""
    test_case = "test_unmap_unaligned_addr"


# XXX disable the test for `memcheck'
# until https://github.com/pmem/pmdk/issues/5600 is fixed.
@t.require_valgrind_disabled('memcheck')
class TEST16(PMEM2_MAP_DEVDAX):
    """DevDax unmap a pmem2 mapping with an unaligned address"""
    test_case = "test_unmap_unaligned_addr"


class TEST18(PMEM2_MAP_NO_FILE):
    """test for pmem2_map_get_address"""
    test_case = "test_map_get_address"


class TEST19(PMEM2_MAP_NO_FILE):
    """test for pmem2_map_get_size"""
    test_case = "test_map_get_size"


class TEST20(PMEM2_MAP_NO_FILE):
    """simply get the previously stored value of granularity"""
    test_case = "test_get_granularity_simple"


class TEST21(PMEM2_MAP):
    """map a file of length which is not page-aligned"""
    test_case = "test_map_unaligned_length"
    filesize = 3 * t.KiB


class TEST22(PMEM2_MAP):
    """map a file which size is not aligned"""
    test_case = "test_map_larger_than_unaligned_file_size"
    filesize = 16 * t.MiB - 1


class TEST23(PMEM2_MAP):
    """
    map a file with zero size, do not provide length
    to pmem2_map config
    """
    test_case = "test_map_zero_file_size"
    filesize = 0


# XXX disable the test for `memcheck'
# until https://github.com/pmem/pmdk/issues/5600 is fixed.
@t.require_valgrind_disabled('memcheck')
class TEST24(PMEM2_MAP):
    """
    map a file with PMEM2_SHARED sharing, changes in the mapping are visible
    in another mapping
    """
    test_case = "test_map_sharing_shared"
    with_size = False


# XXX disable the test for `memcheck'
# until https://github.com/pmem/pmdk/issues/5600 is fixed.
@t.require_valgrind_disabled('memcheck')
class TEST25(PMEM2_MAP):
    """
    map a file with PMEM2_PRIVATE sharing, changes in the mapping are not
    visible in another mapping
    """
    test_case = "test_map_sharing_private"
    with_size = False


# XXX disable the test for `memcheck'
# until https://github.com/pmem/pmdk/issues/5600 is fixed.
@t.require_valgrind_disabled('memcheck')
class TEST26(PMEM2_MAP):
    """
    map a file with PMEM2_PRIVATE sharing, changes in the mapping are not
    visible in another mapping, fd is reopened before each mapping
    """
    test_case = "test_map_sharing_private_with_reopened_fd"
    with_size = False


# XXX disable the test for `memcheck'
# until https://github.com/pmem/pmdk/issues/5600 is fixed.
@t.require_valgrind_disabled('memcheck')
class TEST27(PMEM2_MAP):
    """
    map O_RDONLY file with PMEM2_PRIVATE sharing
    """
    test_case = "test_map_sharing_private_rdonly_file"
    with_size = False


class TEST28(PMEM2_MAP_DEVDAX):
    """DevDax file with PMEM2_PRIVATE sharing"""
    test_case = "test_map_sharing_private_devdax"
    with_size = False


@t.require_architectures('x86_64')
# XXX disable the test for `memcheck'
# until https://github.com/pmem/pmdk/issues/5600 is fixed.
@t.require_valgrind_disabled('memcheck')
class TEST29(PMEM2_MAP):
    """map alignment test for huge pages"""
    test_case = "test_map_huge_alignment"
    filesize = 16 * t.MiB


@t.require_architectures('x86_64')
# XXX disable the test for `memcheck'
# until https://github.com/pmem/pmdk/issues/5600 is fixed.
@t.require_valgrind_disabled('memcheck')
class TEST30(PMEM2_MAP):
    """map alignment test for small pages"""
    test_case = "test_map_huge_alignment"
    filesize = 16 * t.KiB
