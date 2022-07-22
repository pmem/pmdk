#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation
#


import sys

import testframework as t
from consts import MINIASYNC_LIBDIR


@t.freebsd_exclude
class PMEMBLK_FUTURE(t.Test):
    test_type = t.Medium
    block_size = 512
    min_pool_size = 16 * t.MiB + 64 * t.KiB
    lba = 0

    mt = False
    nthreads = 16

    def setup(self, ctx):
        super().setup(ctx)
        if sys.platform == 'win32':
            env_dir = 'PATH'
            path = ctx.env[env_dir]
            ctx.env[env_dir] = path + ";" + MINIASYNC_LIBDIR
        else:
            env_dir = 'LD_LIBRARY_PATH'
            path = ctx.env[env_dir]
            ctx.env[env_dir] = path + ":" + MINIASYNC_LIBDIR
        self.filepath = ctx.create_holey_file(self.min_pool_size, 'testfile')

    def run(self, ctx):
        if self.mt:
            ctx.exec('blk_future', self.test_case, self.filepath,
                     self.block_size, self.nthreads)
        else:
            ctx.exec('blk_future', self.test_case, self.filepath,
                     self.block_size, self.lba)


class TEST0(PMEMBLK_FUTURE):
    """verify pmemblk async write basic functionality"""
    test_case = "test_write_async_basic"


class TEST1(PMEMBLK_FUTURE):
    """
    test pmemblk async write with multiple futures writing to the same block
    from single thread
    """
    test_case = "test_write_async_multiple"


class TEST2(PMEMBLK_FUTURE):
    """verify pmemblk async read basic functionality"""
    test_case = "test_read_async_basic"


class TEST3(PMEMBLK_FUTURE):
    """test pmemblk async write in multithreaded environment"""
    test_case = "test_async_write_mt"
    mt = True
    nthreads = 16


class TEST4(PMEMBLK_FUTURE):
    """
    test pmemblk async write to the same block in multithreaded environment
    """
    test_case = "test_async_write_same_lba_mt"
    mt = True
    nthreads = 16


class TEST5(PMEMBLK_FUTURE):
    """
    test pmemblk async write in environment multithreaded when writing and
    reading the same block
    """
    test_case = "test_async_read_write_same_lba_mt"
    mt = True
    nthreads = 16
