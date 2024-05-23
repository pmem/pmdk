#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation
#


import testframework as t
from testframework import granularity as g
from os import path


SIGABRT_EXIT_CODE = 134


@g.require_granularity(g.ANY)
# The 'debug' build is chosen arbitrarily to ensure these tests are run only
# once. No dynamic libraries are used nor .static_* builds are available.
@t.require_build('debug')
class OBJ_ULOG_ADVANCED(t.Test):
    test_type = t.Short
    test_case = 'test_init_publish_abort_and_verify'
    error_inject = False

    def run(self, ctx):
        testfile = path.join(ctx.testdir, f'testfile{self.testnum}')
        stderr_file = f'err{self.testnum}.log'
        error_inject = 1 if self.error_inject else 0
        # The verify will abort the process when the injected error will be
        # discovered.
        expected_exitcode = SIGABRT_EXIT_CODE if self.error_inject else 0
        ctx.exec('obj_ulog_advanced', self.test_case, testfile, self.slot_num,
                 error_inject, expected_exitcode=expected_exitcode,
                 stderr_file=stderr_file)


class TEST0(OBJ_ULOG_ADVANCED):
    # The number of slots exactly populating a single persistent redo log.
    # Please see the source code for details.
    slot_num = 40


class TEST1(OBJ_ULOG_ADVANCED):
    # The number of slots between the one used by TEST0 and TEST2.
    slot_num = 50


class TEST2(OBJ_ULOG_ADVANCED):
    # The number of slots exactly populating a persistent shadow log without
    # triggering its growth. Please see the source code for details.
    slot_num = 60


class TEST3(TEST1):
    # For details on the injected error please see the source code.
    error_inject = True


class TEST4(TEST2):
    # For details on the injected error please see the source code.
    error_inject = True
