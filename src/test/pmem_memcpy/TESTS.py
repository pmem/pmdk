# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

from collections import namedtuple

import testframework as t


TC = namedtuple('TC', ['dest', 'src', 'length'])


class PmemMemcpy(t.Test):
    test_type = t.Short
    filesize = 4 * t.MiB
    envs = ()
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
        for env in self.envs:
            ctx.env[env] = '1'
        for tc in self.test_cases:
            filepath = ctx.create_holey_file(self.filesize, 'testfile',)
            ctx.exec('pmem_memcpy', filepath,
                     str(tc.dest), str(tc.src), str(tc.length))


class TEST0(PmemMemcpy):
    pass


@t.require_architectures('x86_64')
class TEST1(PmemMemcpy):
    envs = ("PMEM_AVX512F",)


@t.require_architectures('x86_64')
class TEST2(PmemMemcpy):
    envs = ("PMEM_AVX",)


class TEST3(PmemMemcpy):
    envs = ("PMEM_NO_MOVNT",)


class TEST4(PmemMemcpy):
    envs = ("PMEM_NO_MOVNT", "PMEM_NO_GENERIC_MEMCPY")
