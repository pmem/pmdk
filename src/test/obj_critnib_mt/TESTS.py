#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t
from testframework import granularity as g


@g.no_testdir()
class ObjCritnibMt(t.Test):
    def run(self, ctx):
        ctx.exec('obj_critnib_mt')


class TEST0(ObjCritnibMt):
    test_type = t.Medium


@t.require_valgrind_enabled('helgrind')
class TEST1(ObjCritnibMt):
    test_type = t.Long


@t.require_valgrind_enabled('drd')
class TEST2(ObjCritnibMt):
    test_type = t.Long
