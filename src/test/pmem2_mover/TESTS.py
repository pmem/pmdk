#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022-2023, Intel Corporation
#


import testframework as t
from testframework import granularity as g
from consts import MINIASYNC_LIBDIR


class PMEM2_MOVER(t.Test):
    test_type = t.Medium

    def setup(self, ctx):
        super().setup(ctx)
        env_dir = 'LD_LIBRARY_PATH'
        path = ctx.env[env_dir]
        ctx.env[env_dir] = path + ":" + MINIASYNC_LIBDIR
        self.filepath = ctx.create_holey_file(16 * t.MiB, 'testfile')

    def run(self, ctx):
        ctx.exec('pmem2_mover', self.test_case, self.filepath)


@g.require_granularity(g.BYTE, g.CACHELINE)
class PMEM2_MOVER_MT(PMEM2_MOVER):
    thread_num = 2

    def run(self, ctx):
        ctx.exec('pmem2_mover', self.test_case, self.filepath, self.thread_num)


# XXX disable the test for 'pmemcheck'
# until https://github.com/pmem/pmdk/issues/5595 is fixed.
@t.require_valgrind_disabled('pmemcheck')
class TEST0(PMEM2_MOVER):
    """verify pmem2 mover memcpy functionality"""
    test_case = "test_mover_memcpy_basic"


# XXX disable the test for 'pmemcheck'
# until https://github.com/pmem/pmdk/issues/5595 is fixed.
@t.require_valgrind_disabled('pmemcheck')
class TEST1(PMEM2_MOVER):
    """verify pmem2 mover memmove functionality"""
    test_case = "test_mover_memmove_basic"


# XXX disable the test for 'pmemcheck'
# until https://github.com/pmem/pmdk/issues/5595 is fixed.
@t.require_valgrind_disabled('pmemcheck')
class TEST2(PMEM2_MOVER):
    """verify pmem2 mover memset functionality"""
    test_case = "test_mover_memset_basic"


# XXX disable the test for 'pmemcheck'
# until https://github.com/pmem/pmdk/issues/5595 is fixed.
@t.require_valgrind_disabled('pmemcheck')
class TEST3(PMEM2_MOVER_MT):
    """verify pmem2 mover multi-threaded memcpy functionality"""
    test_case = "test_mover_memcpy_multithreaded"


# XXX disable the test for 'pmemcheck'
# until https://github.com/pmem/pmdk/issues/5595 is fixed.
@t.require_valgrind_disabled('pmemcheck')
class TEST4(PMEM2_MOVER_MT):
    """verify pmem2 mover multi-threaded memmove functionality"""
    test_case = "test_mover_memmove_multithreaded"


# XXX disable the test for 'pmemcheck'
# until https://github.com/pmem/pmdk/issues/5595 is fixed.
@t.require_valgrind_disabled('pmemcheck')
class TEST5(PMEM2_MOVER_MT):
    """verify pmem2 mover multi-threaded memset functionality"""
    test_case = "test_mover_memset_multithreaded"


# XXX disable the test for 'pmemcheck' and 'BYTE' granurality
# until https://github.com/pmem/pmdk/issues/5686 is fixed.
class PMEM2_MOVER_MT_TEST6(PMEM2_MOVER_MT):
    test_type = t.Long
    thread_num = 16
    """verify pmem2 mover multi-threaded memcpy functionality (Long)"""
    test_case = "test_mover_memcpy_multithreaded"


# XXX disable the test for 'drd``
# until https://github.com/pmem/pmdk/issues/5694 is fixed.
@t.require_valgrind_disabled('drd')
@g.require_granularity(g.PAGE, g.CACHELINE)
class TEST6(PMEM2_MOVER_MT_TEST6):
    pass


# XXX disable the test for 'drd``
# until https://github.com/pmem/pmdk/issues/5694 is fixed.
@t.require_valgrind_disabled('pmemcheck', 'drd')
@g.require_granularity(g.BYTE)
class TEST61(PMEM2_MOVER_MT_TEST6):
    pass


# XXX disable the test for 'pmemcheck' and 'BYTE' granurality
# until https://github.com/pmem/pmdk/issues/5686 is fixed.
class PMEM2_MOVER_MT_TEST7(PMEM2_MOVER_MT):
    test_type = t.Long
    thread_num = 16
    """verify pmem2 mover multi-threaded memmove functionality (Long)"""
    test_case = "test_mover_memmove_multithreaded"


# XXX disable the test for 'drd``
# until https://github.com/pmem/pmdk/issues/5694 is fixed.
@t.require_valgrind_disabled('drd')
@g.require_granularity(g.PAGE, g.CACHELINE)
class TEST7(PMEM2_MOVER_MT_TEST7):
    pass


# XXX disable the test for 'drd``
# until https://github.com/pmem/pmdk/issues/5694 is fixed.
@t.require_valgrind_disabled('pmemcheck', 'drd')
@g.require_granularity(g.BYTE)
class TEST71(PMEM2_MOVER_MT_TEST7):
    pass


# XXX disable the test for 'pmemcheck' and 'BYTE' granurality
# until https://github.com/pmem/pmdk/issues/5686 is fixed.
class PMEM2_MOVER_MT_TEST8(PMEM2_MOVER_MT):
    test_type = t.Long
    thread_num = 16
    """verify pmem2 mover multi-threaded memset functionality (Long)"""
    test_case = "test_mover_memset_multithreaded"


# XXX disable the test for 'drd``
# until https://github.com/pmem/pmdk/issues/5694 is fixed.
@t.require_valgrind_disabled('drd')
@g.require_granularity(g.PAGE, g.CACHELINE)
class TEST8(PMEM2_MOVER_MT_TEST8):
    pass


# XXX disable the test for 'drd``
# until https://github.com/pmem/pmdk/issues/5694 is fixed.
@t.require_valgrind_disabled('pmemcheck', 'drd')
@g.require_granularity(g.BYTE)
class TEST81(PMEM2_MOVER_MT_TEST8):
    pass


# XXX disable the test for 'pmemcheck'
# until https://github.com/pmem/pmdk/issues/5595 is fixed.
@t.require_valgrind_disabled('pmemcheck')
class TEST9(PMEM2_MOVER):
    """verify pmem2 mover functionality"""
    test_case = "test_miniasync_mover"
