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
from os import path
from pathlib import Path
import futils

import testframework as t


@t.require_build(['debug', 'release'])
class EX_LIBPMEM2(t.Test):
    test_type = t.Medium

    offset = str(97 * t.KiB)
    length = str(65 * t.KiB)

    def get_path(self, ctx, file_name):
        path = futils.get_examples_dir(ctx)
        filepath = ctx.create_non_zero_file(1 * t.MiB,
                                            Path(ctx.testdir, file_name))
        return path, filepath


@t.windows_exclude
class TEST0(EX_LIBPMEM2):

    def run(self, ctx):
        test_path, file_path = self.get_path(ctx, 'testfile0')
        ctx.exec(path.join(test_path, 'libpmem2', 'basic'), file_path)


@t.windows_only
class TEST1(EX_LIBPMEM2):

    def run(self, ctx):
        test_path, file_path = self.get_path(ctx, 'testfile1')
        ctx.exec(path.join(test_path, 'ex_pmem2_basic'), file_path)


@t.windows_exclude
class TEST2(EX_LIBPMEM2):

    def run(self, ctx):
        test_path, file_path = self.get_path(ctx, 'testfile2')
        ctx.exec(path.join(test_path, 'libpmem2', 'advanced'),
                 file_path, self.offset, self.length)


@t.windows_only
class TEST3(EX_LIBPMEM2):

    def run(self, ctx):
        test_path, file_path = self.get_path(ctx, 'testfile3')
        ctx.exec(path.join(test_path, 'ex_pmem2_advanced'),
                 file_path, self.offset, self.length)
