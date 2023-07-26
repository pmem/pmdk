#!../env.py
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2023, Intel Corporation

import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
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


@t.require_devdax(t.DevDax('devdax1'))
class PMEM2_SOURCE_DEVDAX(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ddpath = ctx.devdaxes.devdax1.path
        ctx.exec('pmem2_source', self.test_case, ddpath)


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
    labels = ['fault_injection']
    test_case = "test_alloc_src_enomem"


class TEST5(PMEM2_SOURCE_NO_DIR):
    """deleting null pmem2_source"""
    test_case = "test_delete_null_config"


class TEST11(PMEM2_SOURCE):
    """set directory's fd in the source"""
    test_case = "test_set_directory_fd"

    def run(self, ctx):
        ctx.exec('pmem2_source', self.test_case, ctx.testdir)


class TEST13(PMEM2_SOURCE):
    """get file descriptor from the source"""
    test_case = "test_get_fd"


class TEST15(PMEM2_SOURCE_NO_DIR):
    """get file descriptor from the invalid source type"""
    test_case = "test_get_fd_inval_type"


class TEST16(PMEM2_SOURCE):
    """test mcsafe read operation"""
    test_case = "test_pmem2_src_mcsafe_read"


class TEST17(PMEM2_SOURCE):
    """test mcsafe write operation"""
    test_case = "test_pmem2_src_mcsafe_write"


class TEST18(PMEM2_SOURCE_DEVDAX):
    """test mcsafe read operation"""
    test_case = "test_pmem2_src_mcsafe_read"


class TEST19(PMEM2_SOURCE_DEVDAX):
    """test mcsafe write operation"""
    test_case = "test_pmem2_src_mcsafe_write"


class TEST20(PMEM2_SOURCE):
    """
    test mcsafe read and write operations with length bigger than source size
    """
    test_case = "test_pmem2_src_mcsafe_read_write_len_out_of_range"


class TEST21(PMEM2_SOURCE_DEVDAX):
    """
    test mcsafe read and write operations with length bigger than source size
    on devdax
    """
    test_case = "test_pmem2_src_mcsafe_read_write_len_out_of_range"


class TEST22(PMEM2_SOURCE):
    """
    test mcsafe read and write operations on source with invalid type
    """
    test_case = "test_pmem2_src_mcsafe_read_write_invalid_ftype"


class TEST23(PMEM2_SOURCE_DEVDAX):
    """
    test mcsafe read and write operations on source with invalid type on devdax
    """
    test_case = "test_pmem2_src_mcsafe_read_write_invalid_ftype"
