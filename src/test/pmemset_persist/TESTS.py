#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2022, Intel Corporation
#

import configurator
import testframework as t
from testframework import granularity as g
import futils
import os
import sys

@g.require_granularity(g.ANY)
class PMEMSET_PERSIST(t.Test):
    test_type = t.Short
    create_file = True

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        ctx.exec('pmemset_persist', self.test_case, filepath)


class TEST0(PMEMSET_PERSIST):
    """test pmemset_persist on region in one part"""
    test_case = "test_persist_single_part"


@t.windows_exclude
@t.require_valgrind_enabled('pmemcheck')
class TEST1(PMEMSET_PERSIST):
    """test pmemset_persist on region in one part"""
    test_case = "test_persist_single_part"


class TEST2(PMEMSET_PERSIST):
    """test pmemset_persist on region across more than one part"""
    test_case = "test_persist_multiple_parts"


@t.windows_exclude
@t.require_valgrind_enabled('pmemcheck')
class TEST3(PMEMSET_PERSIST):
    """test pmemset_persist on region across more than one part"""
    test_case = "test_persist_multiple_parts"


@t.windows_exclude
@t.require_valgrind_enabled('pmemcheck')
class TEST4(PMEMSET_PERSIST):
    """test pmemset_persist on incomplete region - should fail"""
    test_case = "test_persist_incomplete"

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        ctx.exec('pmemset_persist', self.test_case, filepath)
        pmemecheck_log = os.path.join(configurator.Configurator().config.dir,
                                      'pmemset_persist', 'pmemcheck4.log')
        futils.tail(pmemecheck_log, 2)


class TEST5(PMEMSET_PERSIST):
    """test pmemset_flush some part and drain """
    test_case = "test_set_flush_drain"


@t.windows_exclude
@t.require_valgrind_enabled('pmemcheck')
class TEST6(PMEMSET_PERSIST):
    """test pmemset_flush some part and drain -
    should fail (not all flushed)"""
    test_case = "test_set_flush_drain"

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        ctx.exec('pmemset_persist', self.test_case, filepath)
        pmemecheck_log = os.path.join(configurator.Configurator().config.dir,
                                      'pmemset_persist', 'pmemcheck6.log')
        futils.tail(pmemecheck_log, 2)
