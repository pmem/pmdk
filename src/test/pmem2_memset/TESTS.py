#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2022, Intel Corporation
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

        if ctx.wc_workaround() == 'on':
            ctx.env['PMEM_WC_WORKAROUND'] = '1'
        elif ctx.wc_workaround() == 'off':
            ctx.env['PMEM_WC_WORKAROUND'] = '0'

        for tc in self.test_cases:
            filepath = ctx.create_holey_file(self.filesize, 'testfile',)
            ctx.exec('pmem2_memset', filepath, tc.offset, tc.length)


@t.add_params('wc_workaround', ['on', 'off', 'default'])
class TEST0(Pmem2Memset):
    pass


@t.require_architectures('x86_64')
@t.add_params('wc_workaround', ['on', 'off', 'default'])
class TEST1(Pmem2Memset):
    envs0 = ("PMEM_AVX512F",)


@t.require_architectures('x86_64')
@t.add_params('wc_workaround', ['on', 'off', 'default'])
class TEST2(Pmem2Memset):
    envs0 = ("PMEM_AVX512F", "PMEM_AVX",)


@t.add_params('wc_workaround', ['default'])
class TEST3(Pmem2Memset):
    envs1 = ("PMEM_NO_MOVNT",)


@t.add_params('wc_workaround', ['default'])
class TEST4(Pmem2Memset):
    envs1 = ("PMEM_NO_MOVNT", "PMEM_NO_GENERIC_MEMCPY")


@t.require_architectures('x86_64')
@t.add_params('wc_workaround', ['on', 'off', 'default'])
class TEST5(Pmem2Memset):
    envs0 = ("PMEM_MOVDIR64B",)


@t.require_architectures('x86_64')
@t.add_params('wc_workaround', ['on', 'off', 'default'])
class TEST6(Pmem2Memset):
    envs0 = ("PMEM_MOVDIR64B", "PMEM_AVX512F",)


@t.require_architectures('x86_64')
@t.add_params('wc_workaround', ['on', 'off', 'default'])
class TEST7(Pmem2Memset):
    envs0 = ("PMEM_MOVDIR64B", "PMEM_AVX512F", "PMEM_AVX",)
