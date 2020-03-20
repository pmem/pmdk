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
    memmove_fn = 'pmem2_memmove'
    memset_fn = 'pmem2_memset'

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile')
        ctx.env['PMREORDER_EMIT_LOG'] = '1'
        ctx.valgrind.add_opt('--log-stores=yes')
        ctx.exec('pmem2_api', self.test_case, filepath)

        log_name = 'pmemcheck{}.log'.format(self.testnum)
        pmemecheck_log = os.path.join(ctx.cwd, log_name)
        memmove_fn_begin_nums = futils.count(
            pmemecheck_log, self.memmove_fn + '.BEGIN')
        memmove_fn_end_nums = futils.count(
            pmemecheck_log, self.memmove_fn + '.END')
        memset_fn_begin_nums = futils.count(
            pmemecheck_log, self.memset_fn + '.BEGIN')
        memset_fn_end_nums = futils.count(
            pmemecheck_log, self.memset_fn + '.END')

        if (memmove_fn_begin_nums != 2 or memset_fn_begin_nums != 1 or
                memmove_fn_end_nums != 2 or memset_fn_end_nums != 1):
            raise futils.Fail(
                'Pattern: {}.BEGIN occurrs {} times. Expected 2.\n'
                'Pattern: {}.BEGIN occurrs {} times. Expected 1.\n'
                'Pattern: {}.END occurrs {} times. Expected 2.\n'
                'Pattern: {}.END occurrs {} times. Expected 1.'
                .format(self.memmove_fn, memmove_fn_begin_nums,
                        self.memset_fn, memset_fn_begin_nums,
                        self.memmove_fn, memmove_fn_end_nums,
                        self.memset_fn, memset_fn_end_nums)
            )


class TEST0(Pmem2ApiLogs):
    """
    test the emission of library and function names to pmemcheck stores log
    """
    test_case = "test_pmem2_api_logs"
