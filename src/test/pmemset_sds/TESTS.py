#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
class PMEMSET_SDS(t.Test):
    test_type = t.Short
    create_file = True
    file_size = 16 * t.MiB

    def run(self, ctx):
        filepath = "not/existing/file"
        if self.create_file:
            filepath = ctx.create_holey_file(self.file_size, 'testfile1')
        ctx.exec('pmemset_sds', self.test_case, filepath)


class TEST0(PMEMSET_SDS):
    """test pmemset_sds allocation with error injection"""
    test_case = "test_source_set_sds_duplicate_enomem"


class TEST1(PMEMSET_SDS):
    """
    create new sds and map a part,
    then modify the usc in SDS and map a part again
    """
    test_case = "test_sds_part_in_use_wrong_usc"


@t.windows_exclude
@t.require_valgrind_enabled('memcheck')
class TEST2(PMEMSET_SDS):
    """
    create new sds and map a part, then modify the usc in SDS and map a part
    again
    """
    test_case = "test_sds_part_in_use_wrong_usc"


class TEST3(PMEMSET_SDS):
    """create new sds modify the usc in SDS and map a part"""
    test_case = "test_sds_part_not_in_use_wrong_usc"


class TEST4(PMEMSET_SDS):
    """
    create new sds and map a part, then modify the device id in SDS and map a
    part again
    """
    test_case = "test_sds_part_in_use_wrong_device_id"


class TEST5(PMEMSET_SDS):
    """create new sds modify the device id in SDS and map a part"""
    test_case = "test_sds_part_not_in_use_wrong_device_id"


class TEST6(PMEMSET_SDS):
    """create new sds and map three parts one by one"""
    test_case = "test_sds_part_multiple_mappings"
