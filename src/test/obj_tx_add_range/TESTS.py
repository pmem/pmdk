#!../env.py
#
# Copyright 2019, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

"""unit tests for pmemobj_tx_add_range and pmemobj_tx_xadd_range"""

from os import path

import testframework as t


class TEST0(t.BaseTest):
    test_type = t.Medium
    pmemcheck = t.DISABLE

    def run(self, ctx):
        testfile = path.join(ctx.testdir, 'testfile0')
        ctx.exec('obj_tx_add_range', testfile, '0')


class TEST1(t.BaseTest):
    test_type = t.Medium
    pmemcheck = t.ENABLE

    def run(self, ctx):
        self.valgrind.add_opt('--mult-stores=no', t.PMEMCHECK)

        testfile = path.join(ctx.testdir, 'testfile1')
        ctx.exec('obj_tx_add_range', testfile, '0')


class TEST2(t.BaseTest):
    test_type = t.Medium
    memcheck = t.DISABLE
    fs = t.Pmem

    def run(self, ctx):
        testfile = path.join(ctx.testdir, 'testfile2')
        ctx.exec('obj_tx_add_range', testfile, '1')


class TEST3(t.BaseTest):
    test_type = t.Medium
    memcheck = t.ENABLE
    build = t.Debug

    def run(self, ctx):
        testfile = path.join(ctx.testdir, 'testfile3')
        ctx.exec('obj_tx_add_range', testfile, '0')
