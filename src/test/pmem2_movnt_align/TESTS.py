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
import os

# XXX: add require x86_64 to python framework


class MOVNT_ALIGN_COMMON(t.Test):
    test_type = t.Short
    filesize = 4 * t.MiB

    def run_cases(self, ctx):
        ctx.exec('pmem2_movnt_align', self.filepath, "C")
        ctx.exec('pmem2_movnt_align', self.filepath, "F")
        ctx.exec('pmem2_movnt_align', self.filepath, "B")
        ctx.exec('pmem2_movnt_align', self.filepath, "S")

    def run(self, ctx):
        self.filepath = ctx.create_holey_file(self.filesize, 'testfile',)
        self.run_cases(ctx)


class PMEM2_MOVNT_ALIGN(MOVNT_ALIGN_COMMON):
    instruction = None
    threshold = None
    threshold_values = ['0', '99999']

    def run(self, ctx):
        if self.instruction:
            ctx.env[self.instruction] = '1'

        if "PMEM_MOVNT_THRESHOLD" in os.environ:
            os.environ.pop('PMEM_MOVNT_THRESHOLD')

        super().run(ctx)
        for tv in self.threshold_values:
            ctx.env['PMEM_MOVNT_THRESHOLD'] = tv
            self.run_cases(ctx)


@t.require_valgrind_enabled('pmemcheck')
class MOVNT_ALIGN_COMMON_VALGRIND(PMEM2_MOVNT_ALIGN):
    test_type = t.Medium

    def run(self, ctx):
        ctx.env['VALGRIND_OPTS'] = "--mult-stores=yes"
        super().run(ctx)


class TEST0(PMEM2_MOVNT_ALIGN):
    pass


class TEST1(PMEM2_MOVNT_ALIGN):
    instruction = "PMEM_AVX512F"


class TEST2(PMEM2_MOVNT_ALIGN):
    instruction = "PMEM_AVX"


class TEST3(MOVNT_ALIGN_COMMON):
    def run(self, ctx):
        ctx.env['PMEM_NO_MOVNT'] = '1'
        ctx.env['PMEM_NO_GENERIC_MEMCPY'] = '1'
        super().run(ctx)


class TEST4(MOVNT_ALIGN_COMMON_VALGRIND):
    pass


class TEST5(MOVNT_ALIGN_COMMON_VALGRIND):
    instruction = "PMEM_AVX512F"


class TEST6(MOVNT_ALIGN_COMMON_VALGRIND):
    instruction = "PMEM_AVX"


class TEST7(MOVNT_ALIGN_COMMON_VALGRIND):
    def run(self, ctx):
        ctx.env['PMEM_NO_MOVNT'] = '1'
        ctx.env['PMEM_NO_GENERIC_MEMCPY'] = '1'
        super().run(ctx)
