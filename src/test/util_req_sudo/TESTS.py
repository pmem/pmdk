#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#
import testframework as t
from testframework import granularity as g


@t.linux_only
@g.no_testdir()
@t.require_sudo
class TEST0(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('sudo', 'id', '-u', absolute_path=True,
                 stdout_file='sudo0.log')
