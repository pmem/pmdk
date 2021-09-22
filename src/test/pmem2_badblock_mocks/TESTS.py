#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2021, Intel Corporation
#

import testframework as t


@t.linux_only
@t.require_ndctl()
class BB_MOCKS_BASIC(t.Test):
    """PART #1 - basic tests"""
    def run(self, ctx):
        test = 'test_basic'
        ctx.exec('pmem2_badblock_mocks', test, ctx.file_type(), ctx.mode())


@t.add_params('file_type', ['reg_file', 'chr_dev'])
@t.add_params('mode', ['no_device'])
class TEST0(BB_MOCKS_BASIC):
    """did not found any matching device"""
    """regular file / character device"""
    pass


@t.add_params('file_type', ['reg_file'])
@t.add_params('mode', ['namespace', 'region'])
class TEST1(BB_MOCKS_BASIC):
    """regular file, namespace mode / region mode"""
    pass


@t.add_params('file_type', ['chr_dev'])
@t.add_params('mode', ['region'])
class TEST2(BB_MOCKS_BASIC):
    """character device, region mode"""
    pass


@t.linux_only
@t.require_ndctl()
class BB_MOCKS_READ_CLEAR(t.Test):
    """PART #2 - test reading and clearing bad blocks"""
    def run(self, ctx):
        test = 'test_read_clear_bb'
        ctx.exec('pmem2_badblock_mocks',
                 test, ctx.file_type(), ctx.mode(), ctx.bb())


@t.add_params('file_type', ['reg_file'])
@t.add_params('mode', ['namespace', 'region'])
@t.add_params('bb', [1, 2, 3, 4])
class TEST3(BB_MOCKS_READ_CLEAR):
    """regular file, namespace mode / region mode"""
    pass


@t.add_params('file_type', ['chr_dev'])
@t.add_params('mode', ['region'])
@t.add_params('bb', [1, 2, 3, 4])
class TEST4(BB_MOCKS_READ_CLEAR):
    """character device, region mode"""
    pass
