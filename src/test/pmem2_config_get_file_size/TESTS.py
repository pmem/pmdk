#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#


import testframework as t
from testframework import granularity as g


@g.no_testdir()
class TEST0(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_config_get_file_size', 'test_notset_fd')


class NormalFile(t.Test):
    test_type = t.Short

    def run(self, ctx):
        filepath = ctx.create_holey_file(self.size, 'testfile')
        ctx.exec('pmem2_config_get_file_size', self.test_case,
                 filepath, str(self.size))


class TEST1(NormalFile):
    test_case = 'test_normal_file_fd'
    size = 0


@t.windows_only
class TEST2(NormalFile):
    test_case = 'test_normal_file_handle'
    size = 0


class TEST3(NormalFile):
    test_case = 'test_normal_file_fd'
    size = 16 * t.MiB


@t.windows_only
class TEST4(NormalFile):
    test_case = 'test_normal_file_handle'
    size = 16 * t.MiB


@g.require_granularity(g.ANY)
@t.windows_exclude
class TEST5(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_config_get_file_size', 'test_directory_fd',
                 ctx.testdir)


@t.windows_only
class TEST6(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_config_get_file_size', 'test_directory_handle',
                 ctx.testdir)


# On Windows fd interface doesn't support temporary files
# FreeBSD doesn't support O_TMPFILE
@t.linux_only
class TEST7(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_config_get_file_size', 'test_tmpfile_fd',
                 ctx.testdir, str(16 * t.MiB))


# XXX doesn't work
# @t.windows_only
# class TEST8(t.Test):
#    test_type = t.Short
#
#    def run(self, ctx):
#        ctx.exec('pmem2_config_get_file_size', 'tmp_file_handle',
#                 ctx.testdir, str(16 * t.MiB))


@t.windows_exclude
@t.require_devdax(t.DevDax('devdax1'))
class TEST9(t.Test):
    test_type = t.Short

    def run(self, ctx):
        dd = ctx.devdaxes.devdax1
        ctx.exec('pmem2_config_get_file_size',
                 'test_normal_file_fd', dd.path, str(dd.size))
