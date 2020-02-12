#!../env.py
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation

import testframework as t
from testframework import granularity as g
import futils
import os


# All test cases in pmem2_persist_valgrind use Valgrind, which is not available
# on Windows systems.
@t.windows_exclude
@t.require_valgrind_enabled('pmemcheck')
# XXX In the match file, there are two possible numbers of errors. It varies
# from compiler to compiler. There should be only one number when pmemcheck
# will be fixed. Please also remove the below requirement after pmemcheck fix.
# https://github.com/pmem/valgrind/pull/76
@g.require_granularity(g.CL_OR_LESS)
class PMEM2_PERSIST(t.Test):
    test_type = t.Medium
    available_granularity = None

    def run(self, ctx):
        filepath = ctx.create_holey_file(2 * t.MiB, 'testfile')
        ctx.exec('pmem2_persist_valgrind', self.test_case, filepath)


class TEST0(PMEM2_PERSIST):
    """persist continuous data in a range of pmem"""
    test_case = "test_persist_continuous_range"


class TEST1(PMEM2_PERSIST):
    """persist discontinuous data in a range of pmem"""
    test_case = "test_persist_discontinuous_range"


class TEST2(PMEM2_PERSIST):
    """persist part of discontinuous data in a range of pmem"""
    test_case = "test_persist_discontinuous_range_partially"

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.KiB, 'testfile')
        ctx.exec('pmem2_persist_valgrind', self.test_case, filepath)
        pmemecheck_log = os.path.join(
            os.getcwd(), 'pmem2_persist_valgrind', 'pmemcheck2.log')
        futils.tail(pmemecheck_log, 2)


class TEST3(PMEM2_PERSIST):
    """persist data in a range of the memory mapped by mmap()"""
    test_case = "test_persist_nonpmem_data"
