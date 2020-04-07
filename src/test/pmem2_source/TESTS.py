#!../env.py
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

import testframework as t
from testframework import granularity as g


@t.require_fs(granularity=g.ANY)
class PMEM2_SOURCE(t.Test):
    test_type = t.Short

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        ctx.exec('pmem2_source', self.test_case, filepath)


@g.no_testdir()
class PMEM2_SOURCE_NO_DIR(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_source', self.test_case)


class TEST0(PMEM2_SOURCE):
    """setting a read + write file descriptor in pmem2_source"""
    test_case = "test_set_rw_fd"


class TEST1(PMEM2_SOURCE):
    """setting a read only file descriptor in pmem2_source"""
    test_case = "test_set_ro_fd"


class TEST2(PMEM2_SOURCE):
    """setting invalid (closed) file descriptor in pmem2_source"""
    test_case = "test_set_invalid_fd"


class TEST3(PMEM2_SOURCE):
    """setting a write only file descriptor in pmem2_source"""
    test_case = "test_set_wronly_fd"


class TEST4(PMEM2_SOURCE):
    """allocation of pmem2_source in case of missing memory in system"""
    test_case = "test_alloc_src_enomem"


class TEST5(PMEM2_SOURCE_NO_DIR):
    """deleting null pmem2_source"""
    test_case = "test_delete_null_config"


@t.windows_only
class TEST6(PMEM2_SOURCE):
    """set handle in the source"""
    test_case = "test_set_handle"


@t.windows_only
class TEST7(PMEM2_SOURCE_NO_DIR):
    """set INVALID_HANLE_VALUE in the source"""
    test_case = "test_set_null_handle"


@t.windows_only
class TEST8(PMEM2_SOURCE):
    """set invalid handle in the source"""
    test_case = "test_set_invalid_handle"


@t.windows_only
class TEST9(PMEM2_SOURCE):
    """set handle to a directory in the source"""
    test_case = "test_set_directory_handle"

    def run(self, ctx):
        ctx.exec('pmem2_source', self.test_case, ctx.testdir)


@t.windows_only
class TEST10(PMEM2_SOURCE_NO_DIR):
    """set handle to a mutex in the source"""
    test_case = "test_set_mutex_handle"


@t.windows_exclude
class TEST11(PMEM2_SOURCE):
    """set directory's fd in the source"""
    test_case = "test_set_directory_fd"

    def run(self, ctx):
        ctx.exec('pmem2_source', self.test_case, ctx.testdir)
