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


class Granularity(str):
    BYTE = '0'
    CACHE_LINE = '1'
    PAGE = '2'


class PMEM2_INTEGRATION(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile')
        ctx.exec('pmem2_integration', self.test_case, filepath)


class PMEM2_GRANULARITY(t.Test):
    test_type = t.Medium
    test_case = 'test_granularity'
    available_granularity = None
    requested_granularity = None

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile')
        ctx.exec('pmem2_integration', self.test_case, filepath,
                 self.available_granularity, self.requested_granularity)


class TEST0(PMEM2_INTEGRATION):
    """map twice using the same config"""
    test_case = "test_reuse_cfg"


class TEST1(PMEM2_INTEGRATION):
    """map using the same config with changed file descriptor"""
    test_case = "test_reuse_cfg_with_diff_fd"

    def run(self, ctx):
        filepath1 = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        filepath2 = ctx.create_holey_file(16 * t.MiB, 'testfile2')
        ctx.exec('pmem2_integration', self.test_case, filepath1, filepath2)


@t.no_testdir()
class TEST2(PMEM2_INTEGRATION):
    """map using the config with default file descriptor"""
    test_case = "test_default_fd"

    def run(self, ctx):
        ctx.exec('pmem2_integration', self.test_case)


# pmem2_config_fd_set does not verify file descriptor mode, because Windows
# does not provide API to do that.
@t.windows_exclude
class TEST3(PMEM2_INTEGRATION):
    """try to change config with file descriptor in write-only mode"""
    test_case = "test_invalid_fd"

    def run(self, ctx):
        filepath1 = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        filepath2 = ctx.create_holey_file(16 * t.MiB, 'testfile2')
        ctx.exec('pmem2_integration', self.test_case, filepath1, filepath2)


@t.require_valgrind_enabled('pmemcheck')
class TEST4(PMEM2_INTEGRATION):
    """check if Valgrind registers data writing on pmem"""
    test_case = "test_register_pmem"


class TEST5(PMEM2_INTEGRATION):
    """create multiple mappings with different offsets and lengths"""
    test_case = "test_use_misc_lens_and_offsets"

    def run(self, ctx):
        filepath = ctx.create_holey_file(1 * t.MiB, 'testfile1')
        ctx.exec('pmem2_integration', self.test_case, filepath)


@t.require_granularity(t.PAGE)
class TEST6(PMEM2_GRANULARITY):
    """test granularity with available page granularity and expected page
    granularity"""
    available_granularity = Granularity.PAGE
    requested_granularity = Granularity.PAGE


@t.require_granularity(t.PAGE)
class TEST7(PMEM2_GRANULARITY):
    """test granularity with available page granularity and expected cache
    line granularity"""
    available_granularity = Granularity.PAGE
    requested_granularity = Granularity.CACHE_LINE


@t.require_granularity(t.PAGE)
class TEST8(PMEM2_GRANULARITY):
    """test granularity with available page granularity and expected byte
    granularity"""
    available_granularity = Granularity.PAGE
    requested_granularity = Granularity.BYTE


@t.require_granularity(t.CACHELINE)
class TEST9(PMEM2_GRANULARITY):
    """test granularity with available cache line granularity and expected
    page granularity"""
    available_granularity = Granularity.CACHE_LINE
    requested_granularity = Granularity.PAGE


@t.require_granularity(t.CACHELINE)
class TEST10(PMEM2_GRANULARITY):
    """test granularity with available cache line granularity and expected
    cache line granularity"""
    available_granularity = Granularity.CACHE_LINE
    requested_granularity = Granularity.CACHE_LINE


@t.require_granularity(t.CACHELINE)
class TEST11(PMEM2_GRANULARITY):
    """test granularity with available cache line granularity and expected
    byte granularity"""
    available_granularity = Granularity.CACHE_LINE
    requested_granularity = Granularity.BYTE


@t.require_granularity(t.BYTE)
class TEST12(PMEM2_GRANULARITY):
    """test granularity with available byte granularity and expected page
    granularity"""
    available_granularity = Granularity.BYTE
    requested_granularity = Granularity.PAGE


@t.require_granularity(t.BYTE)
class TEST13(PMEM2_GRANULARITY):
    """test granularity with available byte granularity and expected cache
    line granularity"""
    available_granularity = Granularity.BYTE
    requested_granularity = Granularity.CACHE_LINE


@t.require_granularity(t.BYTE)
class TEST14(PMEM2_GRANULARITY):
    """test granularity with available byte granularity and expected byte
    granularity"""
    available_granularity = Granularity.BYTE
    requested_granularity = Granularity.BYTE
