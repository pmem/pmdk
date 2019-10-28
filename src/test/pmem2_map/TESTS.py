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
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile1',)
        ctx.exec('pmem2_map', self.test_case, filepath)


class TEST0(PMEM2_MAP):
    """mapping file in read/write mode"""
    test_case = "test_map_rdrw_file"

class TEST1(PMEM2_MAP):
    """mapping file in read mode"""
    test_case = "test_map_rdonly_file"

@t.windows_exclude
class TEST2(PMEM2_MAP):
    """mapping file in access mode"""
    test_case = "test_map_accmode_file"

class TEST3(PMEM2_MAP):
    """ mapping file valid memory range"""
    test_case = "test_map_valid_range_map"

class TEST4(PMEM2_MAP):
    """mapping file beyond valid memory range"""
    test_case = "test_map_invalid_range_map"

class TEST5(PMEM2_MAP):
    """try to mapping file using invalid arguments"""
    test_case = "test_map_invalid_args"

class TEST6(PMEM2_MAP):
    """tries to check if pmem2_unmap was successfull"""
    test_case = "test_unmap_success"

class TEST7(PMEM2_MAP):
    """test tries to unmap unmapped region"""
    test_case = "test_unmap_unmapped_region"
