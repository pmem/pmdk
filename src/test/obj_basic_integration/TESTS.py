#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation


import testframework as t


class BASIC(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        ctx.exec('obj_basic_integration', filepath)


@t.require_valgrind_disabled('memcheck')
class TEST0(BASIC):
    pass


@t.require_valgrind_enabled('pmemcheck')
class TEST1(BASIC):
    pass
