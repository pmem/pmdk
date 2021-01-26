#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2021, Intel Corporation
#


import os

import testframework as t
import futils


@t.require_valgrind_enabled('pmemcheck')
class Pmem2ApiLogs(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile')
        ctx.env['PMREORDER_EMIT_LOG'] = self.PMREORDER_EMIT_LOG
        ctx.valgrind.add_opt('--log-stores=yes')
        ctx.exec('pmem2_api', self.test_case, filepath)

        log_name = 'pmemcheck{}.log'.format(self.testnum)
        pmemecheck_log = os.path.join(ctx.cwd, log_name)
        memmove_fn_begin_nums = futils.count(
            pmemecheck_log, 'pmem2_memmove.BEGIN')
        memmove_fn_end_nums = futils.count(
            pmemecheck_log, 'pmem2_memmove.END')
        memset_fn_begin_nums = futils.count(
            pmemecheck_log, 'pmem2_memset.BEGIN')
        memset_fn_end_nums = futils.count(
            pmemecheck_log, 'pmem2_memset.END')

        if (memmove_fn_begin_nums != self.expected_memmove_fn_nums or
                memmove_fn_end_nums != self.expected_memmove_fn_nums or
                memset_fn_begin_nums != self.expected_memset_fn_nums or
                memset_fn_end_nums != self.expected_memset_fn_nums):
            raise futils.Fail(
                'Pattern: pmem2_memmove.BEGIN occurs {} times. Expected {}.\n'
                'Pattern: pmem2_memmove.END occurs {} times. Expected {}.\n'
                'Pattern: pmem2_memset.BEGIN occurs {} times. Expected {}.\n'
                'Pattern: pmem2_memset.END occurs {} times. Expected {}.'
                .format(memmove_fn_begin_nums, self.expected_memmove_fn_nums,
                        memmove_fn_end_nums, self.expected_memmove_fn_nums,
                        memset_fn_begin_nums, self.expected_memset_fn_nums,
                        memset_fn_end_nums, self.expected_memset_fn_nums)
            )


class TEST0(Pmem2ApiLogs):
    """
    test the emission of library and function names to pmemcheck stores log
    """
    test_case = "test_pmem2_api_logs"
    expected_memmove_fn_nums = 2
    expected_memset_fn_nums = 1
    PMREORDER_EMIT_LOG = '1'


class TEST1(Pmem2ApiLogs):
    """
    test the emission of library and function names to pmemcheck stores log
    with PMREORDER_EMIT_LOG set to '0'. Any names shouldn't be emitted.
    """
    test_case = "test_pmem2_api_logs"
    expected_memmove_fn_nums = 0
    expected_memset_fn_nums = 0
    PMREORDER_EMIT_LOG = '0'
