#!../env.py
#
# Copyright 2019-2020, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
            filesize = str(os.stat(filepath).st_size)
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
            ctx.exec('pmem2_map', self.test_case, dd.path, str(dd.size))
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
    """map using an empty config"""
    test_case = "test_map_empty_config"
    with_size = False


class TEST14(PMEM2_MAP):
    """unmap valid pmem2 mapping"""
    test_case = "test_unmap_valid"


class TEST15(PMEM2_MAP_DEVDAX):
    """DevDax unmap valid pmem2 mapping"""
    test_case = "test_unmap_valid"


# UnmapViewOfFile does not use length
@t.windows_exclude
class TEST16(PMEM2_MAP):
    """unmap a pmem2 mapping with an invalid length"""
    test_case = "test_unmap_zero_length"


class TEST17(PMEM2_MAP_DEVDAX):
    """DevDax unmap a pmem2 mapping with an invalid length"""
    test_case = "test_unmap_zero_length"


# UnmapViewOfFile does not care about the address alignment
@t.windows_exclude
class TEST18(PMEM2_MAP):
    """unmap a pmem2 mapping with an unaligned address"""
    test_case = "test_unmap_unaligned_addr"


class TEST19(PMEM2_MAP_DEVDAX):
    """DevDax unmap a pmem2 mapping with an unaligned address"""
    test_case = "test_unmap_unaligned_addr"


# munmap does not fail if the mapping does not exist
@t.windows_only
class TEST20(PMEM2_MAP):
    """double unmap a pmem2 mapping"""
    test_case = "test_unmap_unmapped"


class TEST21(PMEM2_MAP_NO_FILE):
    """test for pmem2_map_get_address"""
    test_case = "test_map_get_address"


class TEST22(PMEM2_MAP_NO_FILE):
    """test for pmem2_map_get_size"""
    test_case = "test_map_get_size"


class TEST23(PMEM2_MAP_NO_FILE):
    """simply get the previously stored value of granularity"""
    test_case = "test_get_granularity_simple"


class TEST24(PMEM2_MAP):
    """map a file of length which is not page-aligned"""
    test_case = "test_map_unaligned_length"
    filesize = 3 * t.KiB


class TEST25(PMEM2_MAP):
    """map a file which size is not aligned"""
    test_case = "test_map_larger_than_unaligned_file_size"
    filesize = 16 * t.MiB - 1
