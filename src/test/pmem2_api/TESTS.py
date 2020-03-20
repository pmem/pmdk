#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#


import os

import testframework as t
import futils


@t.require_valgrind_enabled('pmemcheck')
class Pmem2ApiLogs(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile')
        ctx.env['PMREORDER_EMIT_LOG'] = '1'
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

        if (memmove_fn_begin_nums != 2 or memmove_fn_end_nums != 2 or
                memset_fn_begin_nums != 1 or memset_fn_end_nums != 1):
            raise futils.Fail(
                'Pattern: pmem2_memmove.BEGIN occurrs {} times. Expected 2.\n'
                'Pattern: pmem2_memmove.END occurrs {} times. Expected 2.\n'
                'Pattern: pmem2_memset.BEGIN occurrs {} times. Expected 1.\n'
                'Pattern: pmem2_memset.END occurrs {} times. Expected 1.'
                .format(memmove_fn_begin_nums, memmove_fn_end_nums,
                        memset_fn_begin_nums, memset_fn_end_nums)
            )


class TEST0(Pmem2ApiLogs):
    """
    test the emission of library and function names to pmemcheck stores log
    """
    test_case = "test_pmem2_api_logs"
