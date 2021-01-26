#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2021, Intel Corporation
#

import testframework as t
from testframework import granularity as g
from testframework import tools
from testframework import futils

import os
import sys


NO_FLAGS = 0
PMEM_F_MEM_NONTEMPORAL = 1
PMEM_F_MEM_TEMPORAL = 2
PMEM_F_MEM_NONTEMPORAL_v_PMEM_F_MEM_TEMPORAL = 3
PMEM_F_MEM_WC = 4
PMEM_F_MEM_WB = 5
PMEM_F_MEM_NOFLUSH = 6
'''
ALL_FLAGS = PMEM_F_MEM_NODRAIN | PMEM_F_MEM_NOFLUSH |
            PMEM_F_MEM_NONTEMPORAL | PMEM_F_MEM_TEMPORAL |
            PMEM_F_MEM_WC | PMEM_F_MEM_WB
'''
ALL_FLAGS = 7

# This match is valid for all cases with a BYTE
# granularity and small (<256) data size.
MATCH_BYTE_SMALL = \
    (
        (NO_FLAGS,                                      128, "t"),
        (PMEM_F_MEM_NONTEMPORAL,                        128, "nt"),
        (PMEM_F_MEM_TEMPORAL,                           128, "t"),
        (PMEM_F_MEM_NONTEMPORAL_v_PMEM_F_MEM_TEMPORAL,  128, "nt"),
        (PMEM_F_MEM_WC,                                 128, "t"),
        (PMEM_F_MEM_WB,                                 128, "t"),
        (PMEM_F_MEM_NOFLUSH,                            128, "t"),
        (ALL_FLAGS,                                     128, "t")
    )

# This match is valid for all cases with a BYTE
# granularity and big (>256) data size.
MATCH_BYTE_BIG = \
    (
        (NO_FLAGS,                                      1024, "t"),
        (PMEM_F_MEM_NONTEMPORAL,                        1024, "nt"),
        (PMEM_F_MEM_TEMPORAL,                           1024, "t"),
        (PMEM_F_MEM_NONTEMPORAL_v_PMEM_F_MEM_TEMPORAL,  1024, "nt"),
        (PMEM_F_MEM_WC,                                 1024, "t"),
        (PMEM_F_MEM_WB,                                 1024, "t"),
        (PMEM_F_MEM_NOFLUSH,                            1024, "t"),
        (ALL_FLAGS,                                     1024, "t")
    )

# This match is valid for all cases with a PAGE/CACHELINE
# granularity and small (<256) data size.
MATCH_PAGE_CACHELINE_SMALL = \
    (
        (NO_FLAGS,                                      128, "t"),
        (PMEM_F_MEM_NONTEMPORAL,                        128, "nt"),
        (PMEM_F_MEM_TEMPORAL,                           128, "t"),
        (PMEM_F_MEM_NONTEMPORAL_v_PMEM_F_MEM_TEMPORAL,  128, "nt"),
        (PMEM_F_MEM_WC,                                 128, "nt"),
        (PMEM_F_MEM_WB,                                 128, "t"),
        (PMEM_F_MEM_NOFLUSH,                            128, "t"),
        (ALL_FLAGS,                                     128, "t")
    )

# This match is valid for all cases with a PAGE/CACHELINE
# granularity and big (>256) data size.
MATCH_PAGE_CACHELINE_BIG = \
    (
        (NO_FLAGS,                                      1024, "nt"),
        (PMEM_F_MEM_NONTEMPORAL,                        1024, "nt"),
        (PMEM_F_MEM_TEMPORAL,                           1024, "t"),
        (PMEM_F_MEM_NONTEMPORAL_v_PMEM_F_MEM_TEMPORAL,  1024, "nt"),
        (PMEM_F_MEM_WC,                                 1024, "nt"),
        (PMEM_F_MEM_WB,                                 1024, "t"),
        (PMEM_F_MEM_NOFLUSH,                            1024, "t"),
        (ALL_FLAGS,                                     1024, "t")
    )

SSE2 = 1
AVX = 2
AVX512 = 3

VARIANT_LIBC = 'libc'
VARIANT_GENERIC = 'generic'
VARIANT_SSE2 = 'sse2'
VARIANT_AVX = 'avx'
VARIANT_AVX512F = 'avx512f'


@t.require_build('debug')
@t.require_architectures('x86_64')
class Pmem2MemExt(t.Test):
    test_type = t.Short
    filesize = 4 * t.MiB

    available_arch = SSE2
    variant = VARIANT_SSE2

    # By default data size is 128 - this is smaller than threshold value (256)
    # to predict usage of temporal stores. This value is overridden in some
    # tests to values bigger than 256.
    data_size = 128

    pmem2_log = ""
    oper = ("C", "M", "S")

    def setup(self, ctx):
        ret = tools.Tools(ctx.env, ctx.build).cpufd()
        self.check_arch(ctx.variant(), ret.returncode)

    def check_arch(self, variant, available_arch):
        if variant == VARIANT_AVX512F:
            if available_arch < AVX512:
                raise futils.Skip("SKIP: AVX512F unavailable")

            # remove this when MSVC we use will support AVX512F
            if sys.platform.startswith('win32'):
                raise futils.Skip("SKIP: AVX512F not supported by MSVC")

            is_avx512f_enabled = tools.envconfig['PMEM2_AVX512F_ENABLED']
            if is_avx512f_enabled == "0":
                raise futils.Skip("SKIP: AVX512F disabled at build time")

        if variant == VARIANT_AVX and available_arch < AVX:
            raise futils.Skip("SKIP: AVX unavailable")

    def check_log(self, ctx, match, type, flag):
        with open(os.path.join(self.cwd, self.pmem2_log), 'r') as f:
            str_val = f.read()

            # count function match, only one log should occur at the time
            count = str_val.count(match)
            if count != 1:
                raise futils.Fail(
                    "Pattern: {} occurs {} times. One expected. "
                    "Type: {} Flag id: {}"
                    .format(match, count, type, flag))

    def create_match(self, variant, oper, store_type):
        match = ""
        if variant == VARIANT_LIBC:
            if oper == "C" or oper == "M":
                match = "memmove_nodrain_libc"
            elif oper == "S":
                match = "memset_nodrain_libc"
            return match

        if variant == VARIANT_GENERIC:
            if oper == "C" or oper == "M":
                match = "memmove_nodrain_generic"
            elif oper == "S":
                match = "memset_nodrain_generic"
            return match

        if oper in ("C", "M"):
            match += "memmove_mov"
        elif oper == "S":
            match += "memset_mov"
        else:
            raise futils.Fail(
                "Operation: {} not supported.".format(oper))

        if store_type == "nt":
            match += store_type

        if variant == VARIANT_SSE2:
            match += "_sse2"
        elif variant == VARIANT_AVX:
            match += "_avx"
        else:
            match += "_avx512f"

        return match

    def run(self, ctx):
        self.pmem2_log = 'pmem2_' + str(self.testnum) + '.log'

        # XXX: add support in the python framework
        # enable pmem2 low level logging
        ctx.env['PMEM2_LOG_FILE'] = self.pmem2_log
        ctx.env['PMEM2_LOG_LEVEL'] = '15'

        if ctx.wc_workaround() == 'on':
            ctx.env['PMEM_WC_WORKAROUND'] = '1'
        elif ctx.wc_workaround() == 'off':
            ctx.env['PMEM_WC_WORKAROUND'] = '0'

        if ctx.variant() == VARIANT_LIBC:
            ctx.env['PMEM_NO_MOVNT'] = '1'
            ctx.env['PMEM_NO_GENERIC_MEMCPY'] = '1'
        elif ctx.variant() == VARIANT_GENERIC:
            ctx.env['PMEM_NO_MOVNT'] = '1'
        elif ctx.variant() == VARIANT_SSE2:
            ctx.env['PMEM_AVX'] = '0'
            ctx.env['PMEM_AVX512F'] = '0'
        elif ctx.variant() == VARIANT_AVX:
            ctx.env['PMEM_AVX'] = '1'
            ctx.env['PMEM_AVX512F'] = '0'
        elif ctx.variant() == VARIANT_AVX512F:
            ctx.env['PMEM_AVX'] = '0'
            ctx.env['PMEM_AVX512F'] = '1'

        filepath = ctx.create_holey_file(self.filesize, 'testfile',)

        for tc in self.test_case:
            for o in self.oper:
                flag_id = tc[0]
                size = tc[1]
                store_type = tc[2]
                match = self.create_match(ctx.variant(), o, store_type)
                ctx.exec('pmem2_mem_ext', filepath, o, size, flag_id)
                self.check_log(ctx, match, o, flag_id)


@t.add_params('variant', [VARIANT_LIBC, VARIANT_GENERIC])
@t.add_params('wc_workaround', ['default'])
class TEST0(Pmem2MemExt):
    test_case = [(NO_FLAGS, 1024, "")]


@g.require_granularity(g.PAGE, g.CACHELINE)
@t.add_params('variant', [VARIANT_SSE2, VARIANT_AVX, VARIANT_AVX512F])
@t.add_params('wc_workaround', ['on', 'off', 'default'])
class TEST1(Pmem2MemExt):
    test_case = MATCH_PAGE_CACHELINE_SMALL


@g.require_granularity(g.BYTE)
@t.add_params('variant', [VARIANT_SSE2, VARIANT_AVX, VARIANT_AVX512F])
@t.add_params('wc_workaround', ['on', 'off', 'default'])
class TEST2(Pmem2MemExt):
    test_case = MATCH_BYTE_SMALL


@g.require_granularity(g.PAGE, g.CACHELINE)
@t.add_params('variant', [VARIANT_SSE2, VARIANT_AVX, VARIANT_AVX512F])
@t.add_params('wc_workaround', ['on', 'off', 'default'])
class TEST3(Pmem2MemExt):
    test_case = MATCH_PAGE_CACHELINE_BIG


@g.require_granularity(g.BYTE)
@t.add_params('variant', [VARIANT_SSE2, VARIANT_AVX, VARIANT_AVX512F])
@t.add_params('wc_workaround', ['on', 'off', 'default'])
class TEST4(Pmem2MemExt):
    test_case = MATCH_BYTE_BIG
