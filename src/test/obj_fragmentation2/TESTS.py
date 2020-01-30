#!../env.py
#
# Copyright 2019-2020, Intel Corporation
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

from os import path

import testframework as t
from testframework import granularity as g
import valgrind as vg


# These tests last too long under drd/helgrind/memcheck/pmemcheck
# Exceptions: workloads no. 6 and 8 under memcheck/pmemcheck
@t.require_valgrind_disabled(['drd', 'helgrind', 'memcheck', 'pmemcheck'])
@g.require_granularity(g.CACHELINE)
@t.require_build('release')
class BASE(t.BaseTest):
    test_type = t.Long
    seed = '12345'
    defrag = '1'

    def run(self, ctx):
        testfile = path.join(ctx.testdir, 'testfile')
        # this test is extremely long otherwise
        ctx.env = {'PMEM_NO_FLUSH': '1'}
        ctx.exec('obj_fragmentation2',
                 testfile, str(self.testnum), self.seed, self.defrag)


class TEST0(BASE):
    pass


class TEST1(BASE):
    pass


class TEST2(BASE):
    pass


class TEST3(BASE):
    pass


class TEST4(BASE):
    pass


class TEST5(BASE):
    pass


class TEST6(BASE):
    # XXX port this to the new framework
    # Restore defaults
    memcheck = vg.AUTO
    pmemcheck = vg.AUTO


class TEST7(BASE):
    pass


class TEST8(BASE):
    # XXX port this to the new framework
    # Restore defaults
    memcheck = vg.AUTO
    pmemcheck = vg.AUTO
