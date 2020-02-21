#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#


import testframework as t


class Pmem2MovntCommon(t.Test):
    test_type = t.Short
    filesize = 4 * t.MiB
    filepath = None

    def create_file(self, ctx):
        self.filepath = ctx.create_holey_file(self.filesize, 'testfile',)


class Pmem2Movnt(Pmem2MovntCommon):
    env_var = None
    threshold = None
    threshold_values = ['1024', '5', '-15']

    def run(self, ctx):
        super().create_file(ctx)
        if self.env_var:
            ctx.env[self.env_var] = '1'

        ctx.exec('pmem2_movnt', self.filepath)
        for tv in self.threshold_values:
            ctx.env['PMEM_MOVNT_THRESHOLD'] = tv
            ctx.exec('pmem2_movnt', self.filepath)


class TEST0(Pmem2Movnt):
    pass


@t.require_architectures('x86_64')
class TEST1(Pmem2Movnt):
    env_var = "PMEM_AVX512F"


@t.require_architectures('x86_64')
class TEST2(Pmem2Movnt):
    env_var = "PMEM_AVX"


class TEST3(Pmem2MovntCommon):
    def run(self, ctx):
        super().create_file(ctx)
        ctx.env['PMEM_NO_MOVNT'] = '1'
        ctx.exec('pmem2_movnt', self.filepath)


class TEST4(Pmem2MovntCommon):
    def run(self, ctx):
        super().create_file(ctx)
        ctx.env['PMEM_NO_MOVNT'] = '1'
        ctx.env['PMEM_NO_GENERIC_MEMCPY'] = '1'
        ctx.exec('pmem2_movnt', self.filepath)
