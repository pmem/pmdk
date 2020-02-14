#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation
#
from os import path
from pathlib import Path
import futils

import testframework as t


@t.require_build(['debug', 'release'])
class EX_LIBPMEM2(t.Test):
    test_type = t.Medium

    offset = str(97 * t.KiB)
    length = str(65 * t.KiB)

    def get_path(self, ctx, file_name, create_file_fn, file_size):
        path = futils.get_examples_dir(ctx)
        filepath = create_file_fn(file_size,
                                  Path(ctx.testdir, file_name))
        return path, filepath


@t.windows_exclude
class TEST0(EX_LIBPMEM2):

    def run(self, ctx):
        test_path, file_path = self.get_path(
            ctx, 'testfile0', ctx.create_non_zero_file, 1 * t.MiB)
        ctx.exec(path.join(test_path, 'libpmem2', 'basic'), file_path)


@t.windows_only
class TEST1(EX_LIBPMEM2):

    def run(self, ctx):
        test_path, file_path = self.get_path(
            ctx, 'testfile1', ctx.create_non_zero_file, 1 * t.MiB)
        ctx.exec(path.join(test_path, 'ex_pmem2_basic'), file_path)


@t.windows_exclude
class TEST2(EX_LIBPMEM2):

    def run(self, ctx):
        test_path, file_path = self.get_path(
            ctx, 'testfile2', ctx.create_non_zero_file, 1 * t.MiB)
        ctx.exec(path.join(test_path, 'libpmem2', 'advanced'),
                 file_path, self.offset, self.length)


@t.windows_only
class TEST3(EX_LIBPMEM2):

    def run(self, ctx):
        test_path, file_path = self.get_path(
            ctx, 'testfile3', ctx.create_non_zero_file, 1 * t.MiB)
        ctx.exec(path.join(test_path, 'ex_pmem2_advanced'),
                 file_path, self.offset, self.length)


class TEST4(EX_LIBPMEM2):

    def run(self, ctx):
        test_path, file_path = self.get_path(
            ctx, 'testfile', ctx.create_holey_file, 16 * t.MiB)
        ctx.exec(path.join(test_path, 'libpmem2', 'persistdata'), file_path,
                 'appendv', '4', 'PMDK ', 'is ', 'the best ', 'open source ',
                 'append', 'project in the world.', 'dump', 'rewind', 'dump',
                 'appendv', '2', 'End of ', 'file.', 'dump',
                 log_file='out4.log')
