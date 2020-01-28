#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t


class TEST0(t.Test):
    test_type = t.Short

    def run(self, ctx):
        filepath1 = ctx.create_holey_file(1 * t.KiB, 'testfile1',)
        filepath2 = ctx.create_holey_file(1 * t.KiB, 'testfile2',)
        '''
        UNITTEST_DO_NOT_CHECK_OPEN_FILES is needed because in test freopen func
        is used
        '''
        ctx.env['UNITTEST_DO_NOT_CHECK_OPEN_FILES'] = '1'
        ctx.exec('pmem2_perror', 'test_simple_check', filepath1, filepath2)


class TEST1(t.Test):
    test_type = t.Short

    def run(self, ctx):
        filepath = ctx.create_holey_file(1 * t.KiB, 'testfile',)
        '''
        UNITTEST_DO_NOT_CHECK_OPEN_FILES is needed because in test freopen func
        is used
        '''
        ctx.env['UNITTEST_DO_NOT_CHECK_OPEN_FILES'] = '1'
        ctx.exec('pmem2_perror', 'test_format_check', filepath)
