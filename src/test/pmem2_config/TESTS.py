#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#


import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
class PMEM2_CONFIG(t.Test):
    test_type = t.Short

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        ctx.exec('pmem2_config', self.test_case, filepath)


@g.no_testdir()
class PMEM2_CONFIG_NO_DIR(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_config', self.test_case)


class TEST0(PMEM2_CONFIG_NO_DIR):
    """allocation and dealocation of pmem2_config"""
    test_case = "test_cfg_create_and_delete_valid"


class TEST1(PMEM2_CONFIG):
    """setting a read + write file descriptor in pmem2_config"""
    test_case = "test_set_rw_fd"


class TEST2(PMEM2_CONFIG):
    """setting a read only file descriptor in pmem2_config"""
    test_case = "test_set_ro_fd"


class TEST3(PMEM2_CONFIG_NO_DIR):
    """resetting file descriptor in pmem2_config"""
    test_case = "test_set_negative_fd"


class TEST4(PMEM2_CONFIG):
    """setting invalid (closed) file descriptor in pmem2_config"""
    test_case = "test_set_invalid_fd"


class TEST5(PMEM2_CONFIG):
    """setting a write only file descriptor in pmem2_config"""
    test_case = "test_set_wronly_fd"


class TEST6(PMEM2_CONFIG_NO_DIR):
    """allocation of pmem2_config in case of missing memory in system"""
    test_case = "test_alloc_cfg_enomem"


class TEST7(PMEM2_CONFIG_NO_DIR):
    """deleting null pmem2_config"""
    test_case = "test_delete_null_config"


class TEST8(PMEM2_CONFIG_NO_DIR):
    """set valid granularity in the config"""
    test_case = "test_config_set_granularity_valid"


class TEST9(PMEM2_CONFIG_NO_DIR):
    """set invalid granularity in the config"""
    test_case = "test_config_set_granularity_invalid"


@t.windows_only
class TEST10(PMEM2_CONFIG):
    """set handle in the config"""
    test_case = "test_set_handle"


@t.windows_only
class TEST11(PMEM2_CONFIG_NO_DIR):
    """set INVALID_HANLE_VALUE in the config"""
    test_case = "test_set_null_handle"


@t.windows_only
class TEST12(PMEM2_CONFIG):
    """set invalid handle in the config"""
    test_case = "test_set_invalid_handle"


@t.windows_only
class TEST13(PMEM2_CONFIG):
    """set handle to a directory in the config"""
    test_case = "test_set_directory_handle"

    def run(self, ctx):
        ctx.exec('pmem2_config', self.test_case, ctx.testdir)


@t.windows_only
class TEST14(PMEM2_CONFIG_NO_DIR):
    """set handle to a mutex in the config"""
    test_case = "test_set_mutex_handle"


@t.windows_exclude
class TEST15(PMEM2_CONFIG):
    """set directory's fd in the config"""
    test_case = "test_set_directory_fd"

    def run(self, ctx):
        ctx.exec('pmem2_config', self.test_case, ctx.testdir)


class TEST16(PMEM2_CONFIG_NO_DIR):
    """setting offset which is too large"""
    test_case = "test_set_offset_too_large"


class TEST17(PMEM2_CONFIG_NO_DIR):
    """setting a valid offset"""
    test_case = "test_set_offset_success"


class TEST18(PMEM2_CONFIG_NO_DIR):
    """setting a valid length"""
    test_case = "test_set_length_success"


class TEST19(PMEM2_CONFIG_NO_DIR):
    """setting maximum possible offset"""
    test_case = "test_set_offset_max"
