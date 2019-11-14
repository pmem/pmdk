#!../env.py
#
# Copyright 2019, Intel Corporation
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


import testframework as t


class PMEM2_MAP(t.BaseTest):
    test_type = t.Short
    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile',)
        ctx.exec('pmem2_map', self.test_case, filepath)


class TEST0(PMEM2_MAP):
    """map a O_RDWR file"""
    test_case = "test_map_rdrw_file"

class TEST1(PMEM2_MAP):
    """map a O_RDONLY file"""
    test_case = "test_map_rdonly_file"

class TEST2(PMEM2_MAP):
    """map a O_WRONLY file"""
    test_case = "test_map_wronly_file"

class TEST3(PMEM2_MAP):
    """map valid memory ranges"""
    test_case = "test_map_valid_ranges"

class TEST4(PMEM2_MAP):
    """map invalid memory ranges"""
    test_case = "test_map_invalid_ranges"

class TEST5(PMEM2_MAP):
    """map using invalid alignment in the offset"""
    test_case = "test_map_invalid_alignment"

class TEST6(PMEM2_MAP):
    """map using a invalid file descriptor"""
    test_case = "test_map_invalid_fd"

class TEST7(PMEM2_MAP):
    """map using an empty config"""
    test_case = "test_map_empty_config"

class TEST8(PMEM2_MAP):
    """unmap valid pmem2 mapping"""
    test_case = "test_unmap_valid"

# UnmapViewOfFile does not use length
@t.windows_exclude
class TEST9(PMEM2_MAP):
    """unmap a pmem2 mapping with an invalid length"""
    test_case = "test_unmap_zero_length"

# UnmapViewOfFile does not care about the address alignment
@t.windows_exclude
class TEST10(PMEM2_MAP):
    """unmap a pmem2 mapping with an unaligned address"""
    test_case = "test_unmap_unaligned_addr"

# munmap does not fail if the mapping does not exist
@t.windows_only
class TEST11(PMEM2_MAP):
    """double unmap a pmem2 mapping"""
    test_case = "test_unmap_unmapped"

class TEST12(PMEM2_MAP):
    """test for pmem2_map_get_address"""
    test_case = "test_map_get_address"

class TEST13(PMEM2_MAP):
    """test for pmem2_map_get_size"""
    test_case = "test_map_get_size"

class TEST14(PMEM2_MAP):
    """simply get the previously stored value of granularity"""
    test_case = "test_get_granularity_simple"
