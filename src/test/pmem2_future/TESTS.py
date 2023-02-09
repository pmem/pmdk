#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022-2023, Intel Corporation
#
import testframework as t
import granularity as g
from consts import MINIASYNC_LIBDIR
import futils
import re


def check_match(matches, expected):
    if len(matches) != len(expected):
        return False
    else:
        for index in range(len(expected)):
            if matches[index][0] != expected[index]:
                return False
    return True


@t.require_build('debug')
class PMEM2_FUTURE(t.Test):
    test_type = t.Short

    def setup(self, ctx):
        super().setup(ctx)
        env_dir = 'LD_LIBRARY_PATH'
        pathh = ctx.env[env_dir]
        ctx.env[env_dir] = pathh + ":" + MINIASYNC_LIBDIR

    def run(self, ctx):
        file_path = ctx.create_holey_file(16 * t.MiB, 'testfile')
        ctx.env['PMEM2_LOG_LEVEL'] = '15'
        ctx.exec('pmem2_future', self.test_case, file_path, self.size)

        log_file = self.get_log_file_by_prefix("pmem2")
        log_content = open(log_file).read()

        # We do not do any checks if the granularity is byte because
        # in the case vdm instances do not have to call
        # anything in order to persist the memory.
        if isinstance(ctx.granularity, g.CacheLine):
            regex = "((pmem2_drain)|(memory_barrier))"
            matches = re.findall(regex, log_content)
            expected = ["pmem2_drain", "memory_barrier"]

            # We expect matches of:
            # [pmem2_drain, memory_barrier] because calling pmem2_memcpy_async
            # should ensure persistence by calling pmem2_drain and
            # memory_barrier after that.
            if check_match(matches, expected) is not True:
                futils.fail(F"Failed to find exact match to "
                            F"pmem2_drain+memory_barrier call!"
                            F"Got {matches} instead.")
        elif isinstance(ctx.granularity, g.Page):
            regex = "pmem2_log_flush"
            matches = re.findall(regex, log_content)

            # We expect exactly one call of pmem2_log_flush which should
            # be called by pmem2_memcpy_async
            if len(matches) != 1:
                futils.fail(F"Failed to find exactly one "
                            F"pmem2_log_flush call! "
                            F"Got {len(matches)} instead.")


# XXX disable the test for 'pmemcheck'
# until https://github.com/pmem/pmdk/issues/5596 is fixed.
@t.require_valgrind_disabled('pmemcheck')
class TEST0(PMEM2_FUTURE):
    size = 64
    test_case = 'test_pmem2_future_mover'


# XXX disable the test for 'pmemcheck'
# until https://github.com/pmem/pmdk/issues/5596 is fixed.
@t.require_valgrind_disabled('pmemcheck')
class TEST1(PMEM2_FUTURE):
    size = 4096
    test_case = 'test_pmem2_future_mover'


# XXX disable the test for 'pmemcheck'
# until https://github.com/pmem/pmdk/issues/5596 is fixed.
@t.require_valgrind_disabled('pmemcheck')
class TEST2(PMEM2_FUTURE):
    size = 64
    test_case = 'test_pmem2_future_vdm'


# XXX disable the test for 'pmemcheck'
# until https://github.com/pmem/pmdk/issues/5596 is fixed.
@t.require_valgrind_disabled('pmemcheck')
class TEST3(PMEM2_FUTURE):
    size = 4096
    test_case = 'test_pmem2_future_vdm'
