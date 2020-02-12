#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation


import testframework as t


@t.require_build(['debug', 'release'])
class BASE(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile')
        filepath1 = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        ctx.exec('obj_ulog_size', filepath, filepath1)


@t.require_valgrind_disabled(['memcheck', 'pmemcheck'])
class TEST0(BASE):
    pass


@t.require_valgrind_enabled('memcheck')
class TEST1(BASE):
    pass


@t.require_valgrind_enabled('pmemcheck')
class TEST2(BASE):
    pass
