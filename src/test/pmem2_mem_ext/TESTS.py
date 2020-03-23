#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t
from testframework import granularity as g
from testframework import tools
from testframework import futils

import os


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


@t.require_build('debug')
@t.require_architectures('x86_64')
class Pmem2MemExt(t.Test):
    test_type = t.Short
    filesize = 4 * t.MiB
    # SSE2
    available_arch = 1

    # By default data size is 128 - this is smaller than threshold value (256)
    # to predict usage of temporal stores. This value is overriden is some tets
    # to value bigger than 256.
    data_size = 128

    pmem2_log = ""
    envs = ()
    arch = "sse2"
    oper = ("C", "M", "S")

    def setup(self, ctx):
        ret = tools.Tools(ctx.env, ctx.build).cpufd()
        self.check_arch(ret.returncode)

    def check_arch(self, available_arch):
        if "PMEM_AVX" in self.envs and available_arch < 2:
            raise futils.Skip("SKIP: AVX unavailable")
        if "PMEM_AVX512F" in self.envs and available_arch < 3:
            raise futils.Skip("SKIP: AVX512F unavailable")

        is_avx512f_enabled = tools.envconfig['PMEM2_AVX512F_ENABLED']
        if is_avx512f_enabled == "0":
            raise futils.Skip("SKIP: AVX512F disabled at build time")

    def check_log(self, ctx, match, type, flag):
        with open(os.path.join(self.cwd, self.pmem2_log), 'r') as f:
            str_val = f.read()

            # count function match, only one log should occurr at the time
            count = str_val.count(match)
            if count != 1:
                raise futils.Fail(
                    "Pattern: {} occurrs {} times. One expected. "
                    "Type: {} Flag id: {}"
                    .format(match, count, type, flag))

    def create_match(self, oper, store_type):
        match = ""
        if "PMEM_NO_MOVNT" in self.envs:
            if "PMEM_NO_GENERIC_MEMCPY" in self.envs:
                if oper == "C" or oper == "M":
                    match = "memmove_nodrain_libc"
                elif oper == "S":
                    match = "memset_nodrain_libc"
            else:
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

        if "PMEM_AVX" in self.envs:
            match += "_avx"
        elif "PMEM_AVX512F" in self.envs:
            match += "_avx512f"
        else:
            match += "_sse2"

        return match

    def run(self, ctx):
        self.pmem2_log = 'pmem2_' + str(self.testnum) + '.log'

        # XXX: add support in the python framework
        # enable pmem2 low level logging
        ctx.env['PMEM2_LOG_FILE'] = self.pmem2_log
        ctx.env['PMEM2_LOG_LEVEL'] = '15'

        for env in self.envs:
            ctx.env[env] = '1'

        filepath = ctx.create_holey_file(self.filesize, 'testfile',)

        for tc in self.test_case:
            for o in self.oper:
                flag_id = tc[0]
                size = tc[1]
                store_type = tc[2]
                match = self.create_match(o, store_type)
                ctx.exec('pmem2_mem_ext', filepath, o, size, flag_id)
                self.check_log(ctx, match, o, flag_id)


class TEST0(Pmem2MemExt):
    test_case = [(NO_FLAGS, 1024, "")]
    envs = ("PMEM_NO_MOVNT", "PMEM_NO_GENERIC_MEMCPY")


class TEST1(Pmem2MemExt):
    test_case = [(NO_FLAGS, 1024, "")]
    envs = ("PMEM_NO_MOVNT",)


@g.require_granularity(g.PAGE, g.CACHELINE)
class TEST2(Pmem2MemExt):
    test_case = MATCH_PAGE_CACHELINE_SMALL


@g.require_granularity(g.BYTE)
class TEST3(Pmem2MemExt):
    test_case = MATCH_BYTE_SMALL


@g.require_granularity(g.PAGE, g.CACHELINE)
class TEST4(Pmem2MemExt):
    test_case = MATCH_PAGE_CACHELINE_BIG


@g.require_granularity(g.BYTE)
class TEST5(Pmem2MemExt):
    test_case = MATCH_BYTE_BIG


@g.require_granularity(g.PAGE, g.CACHELINE)
class TEST6(Pmem2MemExt):
    test_case = MATCH_PAGE_CACHELINE_SMALL
    envs = ("PMEM_AVX",)


@g.require_granularity(g.BYTE)
class TEST7(Pmem2MemExt):
    test_case = MATCH_BYTE_SMALL
    envs = ("PMEM_AVX",)


@g.require_granularity(g.PAGE, g.CACHELINE)
class TEST8(Pmem2MemExt):
    test_case = MATCH_PAGE_CACHELINE_BIG
    envs = ("PMEM_AVX",)


@g.require_granularity(g.BYTE)
class TEST9(Pmem2MemExt):
    test_case = MATCH_BYTE_BIG
    envs = ("PMEM_AVX",)


# Enable Windows tests pmem2_mem_ext when MSVC we use will support AVX512F.
@t.windows_exclude
@g.require_granularity(g.PAGE, g.CACHELINE)
class TEST10(Pmem2MemExt):
    test_case = MATCH_PAGE_CACHELINE_SMALL
    envs = ("PMEM_AVX512F",)


# Enable Windows tests pmem2_mem_ext when MSVC we use will support AVX512F.
@t.windows_exclude
@g.require_granularity(g.BYTE)
class TEST11(Pmem2MemExt):
    test_case = MATCH_BYTE_SMALL
    envs = ("PMEM_AVX512F",)


# Enable Windows tests pmem2_mem_ext when MSVC we use will support AVX512F.
@t.windows_exclude
@g.require_granularity(g.PAGE, g.CACHELINE)
class TEST12(Pmem2MemExt):
    test_case = MATCH_PAGE_CACHELINE_BIG
    envs = ("PMEM_AVX512F",)


# Enable Windows tests pmem2_mem_ext when MSVC we use will support AVX512F.
@t.windows_exclude
@g.require_granularity(g.BYTE)
class TEST13(Pmem2MemExt):
    test_case = MATCH_BYTE_BIG
    envs = ("PMEM_AVX512F",)
