#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#


import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
class Pmem2Config(t.Test):
    test_type = t.Short

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        ctx.exec('pmem2_config', self.test_case, filepath)


@g.no_testdir()
class Pmem2ConfigNoDir(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_config', self.test_case)


class TEST0(Pmem2ConfigNoDir):
    """allocation and dealocation of pmem2_config"""
    test_case = "test_cfg_create_and_delete_valid"


class TEST1(Pmem2ConfigNoDir):
    """allocation of pmem2_config in case of missing memory in system"""
    test_case = "test_alloc_cfg_enomem"


class TEST2(Pmem2ConfigNoDir):
    """deleting null pmem2_config"""
    test_case = "test_delete_null_config"


class TEST3(Pmem2ConfigNoDir):
    """set valid granularity in the config"""
    test_case = "test_config_set_granularity_valid"


class TEST4(Pmem2ConfigNoDir):
    """set invalid granularity in the config"""
    test_case = "test_config_set_granularity_invalid"


class TEST5(Pmem2ConfigNoDir):
    """setting offset which is too large"""
    test_case = "test_set_offset_too_large"


class TEST6(Pmem2ConfigNoDir):
    """setting a valid offset"""
    test_case = "test_set_offset_success"


class TEST7(Pmem2ConfigNoDir):
    """setting a valid length"""
    test_case = "test_set_length_success"


class TEST8(Pmem2ConfigNoDir):
    """setting maximum possible offset"""
    test_case = "test_set_offset_max"


class TEST9(Pmem2ConfigNoDir):
    """setting a valid sharing"""
    test_case = "test_set_sharing_valid"


class TEST10(Pmem2ConfigNoDir):
    """setting a invalid sharing"""
    test_case = "test_set_sharing_invalid"


class TEST11(Pmem2Config):
    """setting unaligned addr and validating it"""
    test_case = "test_validate_unaligned_addr"


class TEST12(Pmem2ConfigNoDir):
    """setting wrong addr request type"""
    test_case = "test_set_wrong_addr_req_type"


class TEST13(Pmem2ConfigNoDir):
    """
    setting null addr when request type PMEM2_ADDRESS_FIXED_NOREPLACE
    is used
    """
    test_case = "test_null_addr_noreplace"


class TEST14(Pmem2ConfigNoDir):
    """
    using pmem2_config_clear_address func
    """
    test_case = "test_clear_address"
