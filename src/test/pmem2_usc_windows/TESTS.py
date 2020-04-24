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
        ctx.exec('pmem2_usc', self.test_case, filepath)


class TEST0(Pmem2Config):
    """test get_volume_handle error handling"""
    test_case = "test_get_volume_handle"


class TEST1(Pmem2Config):
    """test pmem2_source_device_usc error handling"""
    test_case = "test_pmem2_source_device_usc"


class TEST2(Pmem2Config):
    """test pmem2_source_device_id error handling"""
    test_case = "test_pmem2_source_device_id"
