#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation
#


import testframework as t
from testframework import granularity as g


@g.require_granularity(g.ANY)
@t.require_command('dtx_tests')
# The 'debug' build is chosen arbitrarily to ensure these tests are run only
# once. All used binares are provided externally.
@t.require_build('debug')
# There is no multithreading employed in this test.
@t.require_valgrind_disabled('helgrind', 'drd')
class DTX_TESTS(t.Test):
    test_type = t.Short

    def run(self, ctx):
        cmd = ['dtx_tests', '-S', ctx.testdir]
        ctx.exec(*cmd, '-f', 'DTX400.a', cmd_requires_cwd=False)
        ctx.exec(*cmd, '-f', 'DTX400.b', cmd_requires_cwd=False)


@t.require_valgrind_enabled('memcheck')
class TEST0(DTX_TESTS):
    pass


@t.require_valgrind_enabled('pmemcheck')
class TEST1(DTX_TESTS):
    pass
