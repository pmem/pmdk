#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation
#

import sys

import testframework as t
from testframework import granularity as g
from consts import MINIASYNC_LIBDIR


@t.freebsd_exclude
class PMEM2_MOVER(t.Test):
    test_type = t.Medium
    filesize = 16 * t.MiB
    with_size = False

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
        self.filepath = ctx.create_holey_file(self.filesize, 'testfile')

    def run(self, ctx):
        if self.with_size:
            ctx.exec('pmem2_mover', self.test_case, self.filepath,
                     self.filesize)
        else:
            ctx.exec('pmem2_mover', self.test_case, self.filepath)


@t.freebsd_exclude
@t.windows_exclude
@t.require_devdax(t.DevDax('devdax1'))
class PMEM2_MOVER_DEVDAX(t.Test):
    test_type = t.Medium
    with_size = False

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

    def run(self, ctx):
        dd = ctx.devdaxes.devdax1
        if self.with_size:
            ctx.exec('pmem2_mover', self.test_case, dd.path, dd.size)
        else:
            ctx.exec('pmem2_mover', self.test_case, dd.path)


@g.require_granularity(g.BYTE, g.CACHELINE)
class PMEM2_MOVER_MT(PMEM2_MOVER):
    thread_num = 2

    def run(self, ctx):
        ctx.exec('pmem2_mover', self.test_case, self.filepath, self.thread_num)


class TEST0(PMEM2_MOVER):
    """verify pmem2 mover memcpy functionality"""
    test_case = "test_mover_memcpy_basic"


class TEST1(PMEM2_MOVER):
    """verify pmem2 mover memmove functionality"""
    test_case = "test_mover_memmove_basic"


class TEST2(PMEM2_MOVER):
    """verify pmem2 mover memset functionality"""
    test_case = "test_mover_memset_basic"


class TEST3(PMEM2_MOVER_MT):
    """verify pmem2 mover multi-threaded memcpy functionality"""
    test_case = "test_mover_memcpy_multithreaded"


class TEST4(PMEM2_MOVER_MT):
    """verify pmem2 mover multi-threaded memmove functionality"""
    test_case = "test_mover_memmove_multithreaded"


class TEST5(PMEM2_MOVER_MT):
    """verify pmem2 mover multi-threaded memset functionality"""
    test_case = "test_mover_memset_multithreaded"


class TEST6(PMEM2_MOVER_MT):
    test_type = t.Long
    thread_num = 16
    """verify pmem2 mover multi-threaded memcpy functionality (Long)"""
    test_case = "test_mover_memcpy_multithreaded"


class TEST7(PMEM2_MOVER_MT):
    test_type = t.Long
    thread_num = 16
    """verify pmem2 mover multi-threaded memmove functionality (Long)"""
    test_case = "test_mover_memmove_multithreaded"


class TEST8(PMEM2_MOVER_MT):
    test_type = t.Long
    thread_num = 16
    """verify pmem2 mover multi-threaded memset functionality (Long)"""
    test_case = "test_mover_memset_multithreaded"


class TEST9(PMEM2_MOVER):
    """verify pmem2 mover functionality"""
    test_case = "test_miniasync_mover"


class TEST10(PMEM2_MOVER):
    """
    test functionality of pmem2 default mover with the source from existing
    mapping
    """
    test_case = "test_mover_memops_src_existing"
    with_size = True


@g.require_granularity(g.CACHELINE, g.BYTE)
class TEST11(PMEM2_MOVER):
    """
    test functionality of pmem2 default mover with the source on pmem from
    existing mapping
    mapping
    """
    test_case = "test_mover_memops_src_existing_pmem"
    with_size = True


class TEST12(PMEM2_MOVER_DEVDAX):
    """
    test functionality of pmem2 default mover with the devdax source from
    existing mapping
    """
    test_case = "test_mover_memops_src_existing"
    with_size = True
