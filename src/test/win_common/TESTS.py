#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation


import testframework as t


@t.windows_only
class TEST0(t.Test):
    test_type = t.Medium

    def run(self, ctx):
        ctx.exec('win_common', 'setunsetenv')
