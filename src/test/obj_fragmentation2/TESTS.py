#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#

from os import path

import testframework as t
from testframework import granularity as g
import valgrind as vg


# These tests last too long under drd/helgrind/memcheck/pmemcheck
# Exceptions: workloads no. 6 and 8 under memcheck/pmemcheck
@t.require_valgrind_disabled(['drd', 'helgrind', 'memcheck', 'pmemcheck'])
@g.require_granularity(g.CACHELINE)
@t.require_build('release')
class BASE(t.BaseTest):
    test_type = t.Long
    seed = '12345'
    defrag = '1'

    def run(self, ctx):
        testfile = path.join(ctx.testdir, 'testfile')
        # this test is extremely long otherwise
        ctx.env = {'PMEM_NO_FLUSH': '1'}
        ctx.exec('obj_fragmentation2',
                 testfile, str(self.testnum), self.seed, self.defrag)


class TEST0(BASE):
    pass


class TEST1(BASE):
    pass


class TEST2(BASE):
    pass


class TEST3(BASE):
    pass


class TEST4(BASE):
    pass


class TEST5(BASE):
    pass


class TEST6(BASE):
    # XXX port this to the new framework
    # Restore defaults
    memcheck = vg.AUTO
    pmemcheck = vg.AUTO


class TEST7(BASE):
    pass


class TEST8(BASE):
    # XXX port this to the new framework
    # Restore defaults
    memcheck = vg.AUTO
    pmemcheck = vg.AUTO
