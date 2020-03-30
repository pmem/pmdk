#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

from collections import namedtuple

import testframework as t


TC = namedtuple('TC', ['offset', 'length'])


class Pmem2Memset(t.Test):
    test_type = t.Short
    filesize = 4 * t.MiB
    envs0 = ()
    envs1 = ()
    test_cases = (
        TC(offset=0, length=8),
        TC(offset=13, length=4096)
    )

    def run(self, ctx):
        for env in self.envs0:
            ctx.env[env] = '0'
        for env in self.envs1:
            ctx.env[env] = '1'
        for tc in self.test_cases:
            filepath = ctx.create_holey_file(self.filesize, 'testfile',)
            ctx.exec('pmem2_memset', filepath, tc.offset, tc.length)


class TEST0(Pmem2Memset):
    pass


@t.require_architectures('x86_64')
class TEST1(Pmem2Memset):
    envs0 = ("PMEM_AVX512F",)


@t.require_architectures('x86_64')
class TEST2(Pmem2Memset):
    envs0 = ("PMEM_AVX512F", "PMEM_AVX",)


class TEST3(Pmem2Memset):
    envs1 = ("PMEM_NO_MOVNT",)


class TEST4(Pmem2Memset):
    envs1 = ("PMEM_NO_MOVNT", "PMEM_NO_GENERIC_MEMCPY")
