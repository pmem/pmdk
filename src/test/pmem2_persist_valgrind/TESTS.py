#!../env.py
#
# Copyright 2020, Intel Corporation
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

import testframework as t
import futils
import os


class Granularity(str):
    BYTE = '0'
    CACHE_LINE = '1'
    PAGE = '2'


def detect_granularity(ctx):
    if str(ctx.granularity) == 'page':
        available_granularity = Granularity.PAGE
    elif str(ctx.granularity) == 'cacheline':
        available_granularity = Granularity.CACHE_LINE
    elif str(ctx.granularity) == 'byte':
        available_granularity = Granularity.BYTE
    else:
        pass

    return available_granularity


# All test cases in pmem2_persist_valgrind use Valgrind, which is not available
# on Windows systems.
@t.windows_exclude
class PMEM2_PERSIST(t.Test):
    test_type = t.Medium
    available_granularity = None

    def run(self, ctx):
        self.available_granularity = detect_granularity(ctx)
        filepath = ctx.create_holey_file(2 * t.MiB, 'testfile')
        ctx.exec('pmem2_persist_valgrind', self.test_case,
                 filepath, self.available_granularity)


@t.require_valgrind_enabled('pmemcheck')
class TEST0(PMEM2_PERSIST):
    """check if Valgrind registers data writing on pmem"""
    test_case = "test_persist_continuous_range"


@t.require_valgrind_enabled('pmemcheck')
class TEST1(PMEM2_PERSIST):
    """check if Valgrind registers data writing on pmem"""
    test_case = "test_persist_discontinuous_range"


@t.require_valgrind_enabled('pmemcheck')
class TEST2(PMEM2_PERSIST):
    """check if Valgrind registers data writing on pmem"""
    test_case = "test_persist_discontinuous_range_partially"

    def run(self, ctx):
        self.available_granularity = detect_granularity(ctx)
        filepath = ctx.create_holey_file(16 * t.KiB, 'testfile')
        ctx.exec('pmem2_persist_valgrind', self.test_case,
                 filepath, self.available_granularity)
        pmemecheck_log = os.path.join(
            os.getcwd(), 'pmem2_persist_valgrind', 'pmemcheck2.log')
        futils.trim(pmemecheck_log, 2)


@t.require_valgrind_enabled('pmemcheck')
class TEST3(PMEM2_PERSIST):
    """check if Valgrind registers data writing on pmem"""
    test_case = "test_persist_nonpmem_data"
