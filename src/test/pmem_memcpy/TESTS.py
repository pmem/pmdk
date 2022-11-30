# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2022, Intel Corporation

from collections import namedtuple

import testframework as t


TC = namedtuple('TC', ['dest', 'src', 'length'])


class PmemMemcpy(t.Test):
    test_type = t.Short
    filesize = 4 * t.MiB
    envs0 = ()
    envs1 = ()
    test_cases = (
        # aligned everything
        TC(dest=0, src=0, length=4096),

        # unaligned dest
        TC(dest=7, src=0, length=4096),

        # unaligned dest, unaligned src
        TC(dest=7, src=9, length=4096),

        # aligned dest, unaligned src
        TC(dest=0, src=9, length=4096)
    )

    def run(self, ctx):
        for env in self.envs0:
            ctx.env[env] = '0'
        for env in self.envs1:
            ctx.env[env] = '1'
        for tc in self.test_cases:
            filepath = ctx.create_holey_file(self.filesize, 'testfile',)
            ctx.exec('pmem_memcpy', filepath, tc.dest, tc.src, tc.length)


class TEST0(PmemMemcpy):
    pass


class TEST1(PmemMemcpy):
    envs1 = ("PMEM_NO_MOVNT",)


class TEST2(PmemMemcpy):
    envs1 = ("PMEM_NO_MOVNT", "PMEM_NO_GENERIC_MEMCPY")


@t.require_architectures('x86_64')
class TEST3(PmemMemcpy):
    envs0 = ("PMEM_AVX512F",)


@t.require_architectures('x86_64')
class TEST4(PmemMemcpy):
    envs0 = ("PMEM_AVX512F", "PMEM_AVX",)


@t.require_architectures('x86_64')
class TEST5(PmemMemcpy):
    envs1 = ("PMEM_MOVDIR64B",)
