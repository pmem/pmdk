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


class PMEM2_MEMCPY(t.Test):
    test_type = t.Short
    filesize = 4 * t.MiB
    instruction = None
    test_cases = [
        # aligned everything
        [0, 0, 4096],
        # unaligned dest
        [7, 0, 4096],
        # unaligned dest, unaligned src
        [7, 9, 4096],
        # aligned dest, unaligned src
        [0, 9, 4096]]

    def run(self, ctx):
        if self.instruction:
            ctx.env[self.instruction] = '1'
        for tc in self.test_cases:
            filepath = ctx.create_holey_file(self.filesize, 'testfile',)
            ctx.exec('pmem2_memcpy', filepath,
                     str(tc[0]), str(tc[1]), str(tc[2]))


class TEST0(PMEM2_MEMCPY):
    pass


class TEST1(PMEM2_MEMCPY):
    instruction = "PMEM_AVX512F"


class TEST2(PMEM2_MEMCPY):
    instruction = "PMEM_AVX"


class TEST3(PMEM2_MEMCPY):
    instruction = "PMEM_NO_MOVNT"
