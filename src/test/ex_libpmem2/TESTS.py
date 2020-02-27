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

    def set_binary_path(self, test_path, library_name, example_name):
        if platform == 'win32':
            binary_name_args = ['ex', library_name, example_name]
            binary_name = '_'.join(binary_name_args)
            binary_path = path.join(test_path, binary_name)
        else:
            binary_path = path.join(
                test_path, 'lib' + library_name, example_name)

        return binary_path


class TEST0(EX_LIBPMEM2):

    def run(self, ctx):
        example_path = futils.get_examples_path(ctx, 'pmem2', 'basic')
        file_path = ctx.create_non_zero_file(self.file_size, 'testfile0')

        ctx.exec(example_path, file_path)


class TEST1(EX_LIBPMEM2):

    def run(self, ctx):
        example_path = futils.get_examples_path(ctx, 'pmem2', 'advanced')
        file_path = ctx.create_non_zero_file(self.file_size, 'testfile0')

        ctx.exec(example_path, file_path, self.offset, self.length)


class TEST2(EX_LIBPMEM2):
    file_size = 16 * t.MiB

    def run(self, ctx):
        example_path = futils.get_examples_path(ctx, 'pmem2', 'aof')
        file_path = ctx.create_holey_file(self.file_size, 'testfile0')

        args = ['appendv', '4', 'PMDK ', 'is ', 'the best ', 'open source ',
                'append', 'project in the world.', 'dump', 'rewind', 'dump',
                'appendv', '2', 'End of ', 'file.', 'dump']

        ctx.exec(example_path, file_path, *args, stdout_file='out2.log')
