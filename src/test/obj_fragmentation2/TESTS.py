#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#

from os import path

import testframework as t
from testframework import granularity as g


@t.require_fs(granularity=g.CACHELINE)
@t.require_build('release')
class Base(t.Test):
    test_type = t.Long
    seed = '12345'
    defrag = '1'

    def run(self, ctx):
        testfile = path.join(ctx.testdir, 'testfile')
        # this test is extremely long otherwise
        ctx.env['PMEM_NO_FLUSH'] = '1'
        ctx.exec('obj_fragmentation2',
                 testfile, ctx.workload(), self.seed, self.defrag)


# These tests last too long under drd/helgrind/memcheck/pmemcheck
# Exceptions: workloads no. 6 and 8 under memcheck/pmemcheck (run with TEST1)
@t.require_valgrind_disabled('drd', 'helgrind', 'memcheck', 'pmemcheck')
@t.add_params('workload', [0, 1, 2, 3, 4, 5, 7])
class TEST0(Base):
    pass


@t.require_valgrind_disabled('drd', 'helgrind')
@t.add_params('workload', [6, 8])
class TEST1(Base):
    pass
