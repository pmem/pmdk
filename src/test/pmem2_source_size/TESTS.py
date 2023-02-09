#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2023, Intel Corporation
#


import testframework as t


class NormalFile(t.Test):
    test_type = t.Short

    def run(self, ctx):
        filepath = ctx.create_holey_file(self.size, 'testfile')
        ctx.exec('pmem2_source_size', self.test_case,
                 filepath, self.size)


class TEST0(NormalFile):
    test_case = 'test_normal_file_fd'
    size = 0


class TEST2(NormalFile):
    test_case = 'test_normal_file_fd'
    size = 16 * t.MiB


@t.linux_only
class TEST4(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_source_size', 'test_tmpfile_fd',
                 ctx.testdir, 16 * t.MiB)


@t.require_devdax(t.DevDax('devdax1'))
class TEST6(t.Test):
    test_type = t.Short

    def run(self, ctx):
        dd = ctx.devdaxes.devdax1
        ctx.exec('pmem2_source_size',
                 'test_normal_file_fd', dd.path, dd.size)
