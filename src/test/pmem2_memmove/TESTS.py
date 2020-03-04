#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t


class Pmem2Memmove(t.Test):
    test_type = t.Short
    filesize = 4 * t.MiB
    envs = ()
    test_cases = [
        # No offset, no overlap
        ['b:4096'],

        # aligned dest, unaligned source, no overlap
        ['s:7', 'b:4096'],

        # unaligned dest, unaligned source, no overlap
        ['d:7', 's:13', 'b:4096'],

        # all aligned, src overlaps dest
        ['b:4096', 's:23', 'o:1'],

        # unaligned destination
        ['b:4096', 'd:21'],

        # unaligned source and dest
        ['b:4096', 'd:21', 's:7'],

        # overlap of src, aligned src and dest
        ['b:4096', 'o:1', 's:20'],

        # overlap of src, aligned src, unaligned dest
        ['b:4096', 'd:13', 'o:1', 's:20'],

        # dest overlaps src, unaligned dest, aligned src
        ['b:2048', 'd:33', 'o:1'],

        # dest overlaps src, aligned dest and src
        ['b:4096', 'o:1', 'd:20'],

        # aligned dest, no overlap, small length
        ['b:8'],

        # small length, offset 1 byte from 64 byte boundary
        ['b:4', 'd:63'],

        # overlap, src < dest, small length (ensures a copy backwards,
        # with number of bytes to align < length)
        ['o:1', 'd:2', 'b:8']
    ]

    def run(self, ctx):
        for env in self.envs:
            ctx.env[env] = '1'
        for tc in self.test_cases:
            filepath = ctx.create_holey_file(self.filesize, 'testfile',)
            ctx.exec('pmem2_memmove', filepath, *tc)


class TEST0(Pmem2Memmove):
    pass


@t.require_architectures('x86_64')
class TEST1(Pmem2Memmove):
    envs = ("PMEM_AVX512F",)


@t.require_architectures('x86_64')
class TEST2(Pmem2Memmove):
    envs = ("PMEM_AVX",)


class TEST3(Pmem2Memmove):
    envs = ("PMEM_NO_MOVNT",)


class TEST4(Pmem2Memmove):
    envs = ("PMEM_NO_MOVNT", "PMEM_NO_GENERIC_MEMCPY")
