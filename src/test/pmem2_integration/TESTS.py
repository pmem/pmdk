#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#


import testframework as t
from testframework import granularity as g


class Granularity(str):
    BYTE = '0'
    CACHE_LINE = '1'
    PAGE = '2'


class PMEM2_INTEGRATION(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile')
        ctx.exec('pmem2_integration', self.test_case, filepath)


@t.require_devdax(t.DevDax('devdax'))
class PMEM2_INTEGRATION_DEV_DAXES(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        dd = ctx.devdaxes.devdax
        ctx.exec('pmem2_integration', self.test_case, dd.path)


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


@t.require_valgrind_enabled('pmemcheck')
class TEST2(PMEM2_INTEGRATION):
    """check if Valgrind registers data writing on pmem"""
    test_case = "test_register_pmem"


@t.require_valgrind_enabled('pmemcheck')
@t.windows_exclude
class TEST3(PMEM2_INTEGRATION_DEV_DAXES):
    """check if Valgrind registers data writing on DevDax"""
    test_case = "test_register_pmem"


class TEST4(PMEM2_INTEGRATION):
    """create multiple mappings with different offsets and lengths"""
    test_case = "test_use_misc_lens_and_offsets"

    def run(self, ctx):
        filepath = ctx.create_holey_file(1 * t.MiB, 'testfile1')
        ctx.exec('pmem2_integration', self.test_case, filepath)


@g.require_granularity(g.PAGE)
class TEST5(PMEM2_GRANULARITY):
    """test granularity with available page granularity and expected page
    granularity"""
    available_granularity = Granularity.PAGE
    requested_granularity = Granularity.PAGE


@g.require_granularity(g.PAGE)
class TEST6(PMEM2_GRANULARITY):
    """test granularity with available page granularity and expected cache
    line granularity"""
    available_granularity = Granularity.PAGE
    requested_granularity = Granularity.CACHE_LINE


@g.require_granularity(g.PAGE)
class TEST7(PMEM2_GRANULARITY):
    """test granularity with available page granularity and expected byte
    granularity"""
    available_granularity = Granularity.PAGE
    requested_granularity = Granularity.BYTE


@g.require_granularity(g.CACHELINE)
class TEST8(PMEM2_GRANULARITY):
    """test granularity with available cache line granularity and expected
    page granularity"""
    available_granularity = Granularity.CACHE_LINE
    requested_granularity = Granularity.PAGE


@g.require_granularity(g.CACHELINE)
class TEST9(PMEM2_GRANULARITY):
    """test granularity with available cache line granularity and expected
    cache line granularity"""
    available_granularity = Granularity.CACHE_LINE
    requested_granularity = Granularity.CACHE_LINE


@g.require_granularity(g.CACHELINE)
class TEST10(PMEM2_GRANULARITY):
    """test granularity with available cache line granularity and expected
    byte granularity"""
    available_granularity = Granularity.CACHE_LINE
    requested_granularity = Granularity.BYTE


@g.require_granularity(g.BYTE)
class TEST11(PMEM2_GRANULARITY):
    """test granularity with available byte granularity and expected page
    granularity"""
    available_granularity = Granularity.BYTE
    requested_granularity = Granularity.PAGE


@g.require_granularity(g.BYTE)
class TEST12(PMEM2_GRANULARITY):
    """test granularity with available byte granularity and expected cache
    line granularity"""
    available_granularity = Granularity.BYTE
    requested_granularity = Granularity.CACHE_LINE


@g.require_granularity(g.BYTE)
class TEST13(PMEM2_GRANULARITY):
    """test granularity with available byte granularity and expected byte
    granularity"""
    available_granularity = Granularity.BYTE
    requested_granularity = Granularity.BYTE


class TEST14(PMEM2_INTEGRATION):
    """test not aligned length"""
    test_case = "test_len_not_aligned"


@t.windows_exclude
class TEST15(PMEM2_INTEGRATION_DEV_DAXES):
    """test not aligned length on DevDax"""
    test_case = "test_len_not_aligned"


class TEST16(PMEM2_INTEGRATION):
    """test aligned length"""
    test_case = "test_len_aligned"


@t.windows_exclude
class TEST17(PMEM2_INTEGRATION_DEV_DAXES):
    """test aligned length on DevDax"""
    test_case = "test_len_aligned"


class TEST18(PMEM2_INTEGRATION):
    """test unaligned offset"""
    test_case = "test_offset_not_aligned"


@t.windows_exclude
class TEST19(PMEM2_INTEGRATION_DEV_DAXES):
    """test unaligned offset"""
    test_case = "test_offset_not_aligned"


class TEST20(PMEM2_INTEGRATION):
    """test unaligned offset"""
    test_case = "test_offset_aligned"


@t.windows_exclude
class TEST21(PMEM2_INTEGRATION_DEV_DAXES):
    """test unaligned offset"""
    test_case = "test_offset_aligned"


class TEST22(PMEM2_INTEGRATION):
    """
    map O_RDONLY file and test pmem2_[cpy|set|move]_fns with
    PMEM2_PRIVATE sharing
    """
    test_case = "test_mem_move_cpy_set_with_map_private"


class TEST23(PMEM2_INTEGRATION):
    """test valid case of pmem2_deep_sflush"""
    test_case = "test_deep_flush_valid"


class TEST24(PMEM2_INTEGRATION):
    """test deep flush with range out of map"""
    test_case = "test_deep_flush_e_range_behind"


class TEST25(PMEM2_INTEGRATION):
    """test deep flush with range out of map"""
    test_case = "test_deep_flush_e_range_before"


class TEST26(PMEM2_INTEGRATION):
    """test deep flush with part of map"""
    test_case = "test_deep_flush_slice"


class TEST27(PMEM2_INTEGRATION):
    """test deep flush with overlaping part"""
    test_case = "test_deep_flush_overlap"


# XXX: add test cases with:
# @t.require_devdax(t.DevDax('devdax', deep_flush=True))
# @t.require_devdax(t.DevDax('devdax', deep_flush=False))
# if deep_flush == 1 then expected return code 0
# if deep_flush == 0 then expected return code PMEM2_E_NOSUPP
