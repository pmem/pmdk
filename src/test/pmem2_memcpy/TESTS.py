#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

from collections import namedtuple

import testframework as t


TC = namedtuple('TC', ['dest', 'src', 'length'])


class Pmem2Memcpy(t.Test):
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

        if ctx.wc_workaround() == 'on':
            ctx.env['PMEM_WC_WORKAROUND'] = '1'
        elif ctx.wc_workaround() == 'off':
            ctx.env['PMEM_WC_WORKAROUND'] = '0'

        for tc in self.test_cases:
            filepath = ctx.create_holey_file(self.filesize, 'testfile',)
            ctx.exec('pmem2_memcpy', filepath,
                     tc.dest, tc.src, tc.length)


@t.add_params('wc_workaround', ['on', 'off', 'default'])
class TEST0(Pmem2Memcpy):
    pass


@t.require_architectures('x86_64')
@t.add_params('wc_workaround', ['on', 'off', 'default'])
class TEST1(Pmem2Memcpy):
    envs = ("PMEM_AVX512F",)


@t.require_architectures('x86_64')
@t.add_params('wc_workaround', ['on', 'off', 'default'])
class TEST2(Pmem2Memcpy):
    envs = ("PMEM_AVX",)


@t.add_params('wc_workaround', ['default'])
class TEST3(Pmem2Memcpy):
    envs = ("PMEM_NO_MOVNT",)


@t.add_params('wc_workaround', ['default'])
class TEST4(Pmem2Memcpy):
    envs = ("PMEM_NO_MOVNT", "PMEM_NO_GENERIC_MEMCPY")
