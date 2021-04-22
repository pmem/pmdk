#!../envy
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2021, Intel Corporation
#


import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
@g.no_testdir()
class PmemSetConfigNoDir(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmemset_config', self.test_case)


class TEST0(PmemSetConfigNoDir):
    """allocation and dealocation of pmemset_config"""
    test_case = "test_cfg_create_and_delete_valid"


class TEST1(PmemSetConfigNoDir):
    """allocation of pmemset_config in case of missing memory in system"""
    test_case = "test_alloc_cfg_enomem"


class TEST2(PmemSetConfigNoDir):
    """deleting null pmemset_config"""
    test_case = "test_delete_null_config"


class TEST3(PmemSetConfigNoDir):
    """replication of pmemset_config in case of missing memory in system"""
    test_case = "test_duplicate_cfg_enomem"


class TEST4(PmemSetConfigNoDir):
    """pmemset_config invalid store granularity"""
    test_case = "test_set_invalid_granularity"


class TEST5(PmemSetConfigNoDir):
    """pmemset_config tests events"""
    test_case = "test_config_set_event"
