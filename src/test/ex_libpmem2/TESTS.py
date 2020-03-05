#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#
from os import path
from sys import platform

import futils
import testframework as t


@t.require_build(['debug', 'release'])
class EX_LIBPMEM2(t.Test):
    test_type = t.Medium
    file_size = 1 * t.MiB

    offset = str(97 * t.KiB)
    length = str(65 * t.KiB)


@t.windows_exclude
class TEST0(EX_LIBPMEM2):

    def run(self, ctx):
        test_path = futils.get_examples_dir(ctx)
        file_path = ctx.create_non_zero_file(self.file_size, 'testfile0')

        ctx.exec(path.join(test_path, 'libpmem2', 'basic'), file_path)


@t.windows_only
class TEST1(EX_LIBPMEM2):

    def run(self, ctx):
        test_path = futils.get_examples_dir(ctx)
        file_path = ctx.create_non_zero_file(self.file_size, 'testfile0')

        ctx.exec(path.join(test_path, 'ex_pmem2_basic'), file_path)


@t.windows_exclude
class TEST2(EX_LIBPMEM2):

    def run(self, ctx):
        test_path = futils.get_examples_dir(ctx)
        file_path = ctx.create_non_zero_file(self.file_size, 'testfile0')

        ctx.exec(path.join(test_path, 'libpmem2', 'advanced'),
                 file_path, self.offset, self.length)


@t.windows_only
class TEST3(EX_LIBPMEM2):

    def run(self, ctx):
        test_path = futils.get_examples_dir(ctx)
        file_path = ctx.create_non_zero_file(self.file_size, 'testfile0')

        ctx.exec(path.join(test_path, 'ex_pmem2_advanced'),
                 file_path, self.offset, self.length)


class TEST4(EX_LIBPMEM2):
    file_size = 16 * t.MiB

    def run(self, ctx):
        test_path = futils.get_examples_dir(ctx)
        file_path = ctx.create_holey_file(self.file_size, 'testfile0')

        if platform == 'win32':
            binary_path = path.join(test_path, 'ex_pmem2_log')
        else:
            binary_path = path.join(test_path, 'libpmem2', 'log')

        args = ['appendv', '4', 'PMDK ', 'is ', 'the best ', 'open source ',
                'append', 'project in the world.', 'dump', 'rewind', 'dump',
                'appendv', '2', 'End of ', 'file.', 'dump']

        ctx.exec(binary_path, file_path, *args, stdout_file='out4.log')
