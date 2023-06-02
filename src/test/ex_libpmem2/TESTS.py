#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2023, Intel Corporation
#
import futils
import testframework as t
from testframework import granularity as g


@t.require_build(['debug', 'release'])
class EX_LIBPMEM2(t.Test):
    test_type = t.Medium
    file_size = 1 * t.MiB

    offset = str(97 * t.KiB)
    length = str(65 * t.KiB)


class TEST0(EX_LIBPMEM2):

    def run(self, ctx):
        example_path = futils.get_example_path(ctx, 'pmem2', 'basic')
        file_path = ctx.create_non_zero_file(self.file_size, 'testfile0')

        ctx.exec(example_path, file_path)


class TEST1(EX_LIBPMEM2):

    def run(self, ctx):
        example_path = futils.get_example_path(ctx, 'pmem2', 'advanced')
        file_path = ctx.create_non_zero_file(self.file_size, 'testfile0')

        ctx.exec(example_path, file_path, self.offset, self.length)


class TEST2(EX_LIBPMEM2):
    file_size = 16 * t.MiB

    def run(self, ctx):
        example_path = futils.get_example_path(ctx, 'pmem2', 'log')
        file_path = ctx.create_holey_file(self.file_size, 'testfile0')

        args = ['appendv', '4', 'PMDK ', 'is ', 'the best ', 'open source ',
                'append', 'project in the world.', 'dump', 'rewind', 'dump',
                'appendv', '2', 'End of ', 'file.', 'dump']

        ctx.exec(example_path, file_path, *args, stdout_file='out2.log')


class TEST3(EX_LIBPMEM2):

    def run(self, ctx):
        example_path = futils.get_example_path(ctx, 'pmem2', 'redo')
        file_path = ctx.create_holey_file(self.file_size, 'testfile0')
        for x in range(1, 100):
            ctx.exec(example_path, "add", file_path, x, x)
        ctx.exec(example_path, "check", file_path)
        ctx.exec(example_path, "print", file_path, stdout_file='out3.log')


class TEST4(EX_LIBPMEM2):

    def run(self, ctx):
        example_path = futils.get_example_path(ctx, 'pmem2',
                                               'map_multiple_files')

        args = []
        for x in range(1, 10):
            file_path = ctx.create_holey_file(self.file_size,
                                              'testfile{}'.format(x))
            args.append(file_path)

        ctx.exec(example_path, *args, stdout_file='out4.log')


# XXX Disable the test execution under pmemcheck with g.PAGE until the issue
# https://github.com/pmem/pmdk/issues/5641 is fixed.
# additionall test TEST501 has been added to cover non-pmemcheck configs.
class EX_LIBPMEM2_TEST5(EX_LIBPMEM2):

    def run(self, ctx):
        example_path = futils.get_example_path(ctx, 'pmem2', 'unsafe_shutdown')
        file_path = ctx.create_holey_file(self.file_size, 'testfile0')
        ctx.exec(example_path, "write", file_path, "foobar")
        ctx.exec(example_path, "read", file_path, stdout_file='out5.log')


@g.require_granularity(g.CACHELINE, g.BYTE)  # to be removed when fixed
@t.require_valgrind_enabled('pmemcheck')  # to be removed when fixed
class TEST5(EX_LIBPMEM2_TEST5):

    pass


# XXX Disable the test execution with 'memcheck' until the issue:
# https://github.com/pmem/pmdk/issues/5635 is fixed.
# additionall test TEST501 has been added to cover non-pmemcheck configs.
# @t.require_valgrind_disabled('memcheck')  # to be removed when fixed
# @t.require_valgrind_disabled('pmemcheck')
@t.require_valgrind_disabled('pmemcheck', 'memcheck')
class TEST501(EX_LIBPMEM2_TEST5):  # to be removed when fixed

    pass


# XXX disable the test for `memcheck' and 'helgrind'
# until https://github.com/pmem/pmdk/issues/5638 is fixed.
# @t.require_valgrind_disabled('memcheck', 'helgrind')
# XXX disable the test for `drd'
# until https://github.com/pmem/pmdk/issues/5593 is fixed.
# @t.require_valgrind_disabled('drd')
# This test case would require two VALGRIND_SET_CLEAN() calls
# to be added to the "src/examples/libpmem2/ringbuf/ringbuf.c"
# example (see https://github.com/pmem/pmdk/pull/5604)
# in order to pass under pmemcheck, but examples
# do not use valgrind macros on purpose (to avoid unnecessary
# complication), so this test case just should not be run under pmemcheck.
# @t.require_valgrind_disabled('pmemcheck')
# XXX _disabled() can be used only once.
@t.require_valgrind_disabled('memcheck', 'drd', 'pmemcheck', 'helgrind')
class TEST6(EX_LIBPMEM2):

    def run(self, ctx):
        example_path = futils.get_example_path(ctx, 'pmem2', 'ringbuf')
        file_path = ctx.create_holey_file(self.file_size, 'testfile0')
        ctx.exec(example_path, file_path, 10000, 4096, stdout_file='out6.log')
