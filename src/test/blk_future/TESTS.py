#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation
#


import sys

import testframework as t
from consts import MINIASYNC_LIBDIR


@t.freebsd_exclude
class PMEMOBJ_MOVER(t.Test):
    test_type = t.Medium
    block_size = 512
    min_pool_size = 16 * t.MiB + 64 * t.KiB
    lba = 0
    operation = 'c'

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
        ctx.exec('blk_future', self.test_case, self.filepath, self.block_size,
                 self.lba)


class TEST0(PMEMOBJ_MOVER):
    """verify pmemblk async write basic functionality"""
    test_case = "test_write_async_basic"


class TEST1(PMEMOBJ_MOVER):
    """verify pmemblk async read basic functionality"""
    test_case = "test_read_async_basic"
