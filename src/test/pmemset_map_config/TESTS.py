#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
class PMEMSET_MAP_CONFIG(t.Test):
    test_type = t.Short
    create_file = True
    file_size = 16 * t.MiB

    def run(self, ctx):
        filepath = "not/existing/file"
        if self.create_file:
            filepath = ctx.create_holey_file(self.file_size, 'testfile1')
        ctx.exec('pmemset_map_config', self.test_case, filepath)


class TEST0(PMEMSET_MAP_CONFIG):
    """test pmemset_part allocation with error injection"""
    test_case = "test_map_config_new_enomem"


class TEST1(PMEMSET_MAP_CONFIG):
    """create a new part from a source with valid path assigned"""
    test_case = "test_map_config_new_valid_source_file"


class TEST2(PMEMSET_MAP_CONFIG):
    """create a new part from a source with valid pmem2_source assigned"""
    test_case = "test_map_config_new_valid_source_pmem2"


class TEST3(PMEMSET_MAP_CONFIG):
    """delete null pmemset_map_config"""
    create_file = False
    test_case = "test_delete_null_config"


class TEST4(PMEMSET_MAP_CONFIG):
    """try to set invalid source to map config"""
    test_case = "test_map_config_new_invalid_source"


class TEST5(PMEMSET_MAP_CONFIG):
    """try to set invalid offset to map config"""
    test_case = "test_map_config_invalid_offset"
