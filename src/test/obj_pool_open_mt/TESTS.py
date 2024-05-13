#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2024, Intel Corporation
#

import testframework as t


class BASE(t.Test):
    test_type = t.Medium
    niter = 2

    def run(self, ctx):
        ctx.exec('obj_pool_open_mt', ctx.testdir, self.niter)


class TEST0(BASE):
    "multi-threaded open test"


@t.require_valgrind_enabled('helgrind')
class TEST1(BASE):
    "multi-threaded open test /w helgrind"


class TEST2(BASE):
    niter = 8
    "multi-threaded open test /w 8 iterations"
