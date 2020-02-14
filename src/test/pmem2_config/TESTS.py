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


class TEST1(PMEM2_CONFIG_NO_DIR):
    """allocation of pmem2_config in case of missing memory in system"""
    test_case = "test_alloc_cfg_enomem"


class TEST2(PMEM2_CONFIG_NO_DIR):
    """deleting null pmem2_config"""
    test_case = "test_delete_null_config"


class TEST3(PMEM2_CONFIG_NO_DIR):
    """set valid granularity in the config"""
    test_case = "test_config_set_granularity_valid"


class TEST4(PMEM2_CONFIG_NO_DIR):
    """set invalid granularity in the config"""
    test_case = "test_config_set_granularity_invalid"


class TEST5(PMEM2_CONFIG_NO_DIR):
    """setting offset which is too large"""
    test_case = "test_set_offset_too_large"


class TEST6(PMEM2_CONFIG_NO_DIR):
    """setting a valid offset"""
    test_case = "test_set_offset_success"


class TEST7(PMEM2_CONFIG_NO_DIR):
    """setting a valid length"""
    test_case = "test_set_length_success"


class TEST8(PMEM2_CONFIG_NO_DIR):
    """setting maximum possible offset"""
    test_case = "test_set_offset_max"
