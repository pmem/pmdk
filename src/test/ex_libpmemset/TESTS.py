#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#
import futils
import testframework as t
from testframework import granularity as g


@t.require_build(['debug', 'release'])
class EX_LIBPMEMSET(t.Test):
    test_type = t.Medium
    file_size = 1 * t.MiB


class TEST0(EX_LIBPMEMSET):

    def run(self, ctx):
        example_path = futils.get_example_path(ctx, 'pmemset', 'basic')
        file_path = ctx.create_non_zero_file(self.file_size, 'testfile0')

        ctx.exec(example_path, file_path)


@g.require_granularity(g.CACHELINE, g.BYTE)
class TEST1(EX_LIBPMEMSET):

    def run(self, ctx):
        example_path = futils.get_example_path(ctx, 'pmemset', 'log')
        log_dir = ctx.mkdirs('log')
        rep_dir = ctx.mkdirs('rep')

        ctx.exec(example_path, log_dir, rep_dir, 'c')
