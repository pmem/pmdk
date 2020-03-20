#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#


import testframework as t
from testframework import granularity as g
import futils
import os


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


@t.require_valgrind_enabled('pmemcheck')
class PMEM2_API_LOGS(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile')
        ctx.env['PMREORDER_EMIT_LOG'] = '1'
        ctx.valgrind.add_opt('--log-stores=yes')
        ctx.exec('pmem2_integration', self.test_case, filepath)

        log_name_elems = ['pmemcheck', self.test_number, '.log']
        pmemecheck_log = os.path.join(
            os.getcwd(), 'pmem2_integration', ('').join(log_name_elems))
        memmove_fn_begin_nums = futils.count(
            pmemecheck_log, self.memmove_fn + '.BEGIN')
        memmove_fn_end_nums = futils.count(
            pmemecheck_log, self.memmove_fn + '.END')
        memset_fn_begin_nums = futils.count(
            pmemecheck_log, self.memset_fn + '.BEGIN')
        memset_fn_end_nums = futils.count(
            pmemecheck_log, self.memset_fn + '.END')

        if (memmove_fn_begin_nums != 2 or memset_fn_begin_nums != 1 or memmove_fn_end_nums != 2 or memset_fn_end_nums != 1):
            raise futils.Fail(
                'Pattern: {}.BEGIN occurrs {} times. Expected 1.\n'
                'Pattern: {}.END occurrs {} times. Expected 1.\n'
                'Pattern: {}.BEGIN occurrs {} times. Expected 2.\n'
                'Pattern: {}.END occurrs {} times. Expected 2.'
                .format(self.memset_fn, memset_fn_begin_nums,
                        self.memset_fn, memset_fn_end_nums,
                        self.memmove_fn, memmove_fn_begin_nums,
                        self.memmove_fn, memmove_fn_end_nums)
            )


@g.require_granularity(g.PAGE)
class TEST23(PMEM2_API_LOGS):
    """
    test the emission of library and function names to pmemcheck store log
    for page granularity
    """
    test_case = "test_pmem2_api_logs"
    test_number = '23'
    memmove_fn = 'pmem2_memmove_nonpmem'
    memset_fn = 'pmem2_memset_nonpmem'


@g.require_granularity(g.CACHELINE)
class TEST24(PMEM2_API_LOGS):
    """
    test the emission of library and function names to pmemcheck store log
    for cacheline granularity
    """
    test_case = "test_pmem2_api_logs"
    test_number = '24'
    memmove_fn = 'pmem2_memmove'
    memset_fn = 'pmem2_memset'


@g.require_granularity(g.BYTE)
class TEST25(PMEM2_API_LOGS):
    """
    test the emission of library and function names to pmemcheck store log
    for byte granularity
    """
    test_case = "test_pmem2_api_logs"
    test_number = '25'
    memmove_fn = 'pmem2_memmove_eadr'
    memset_fn = 'pmem2_memset_eadr'
