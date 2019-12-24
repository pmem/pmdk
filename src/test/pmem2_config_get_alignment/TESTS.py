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


import testframework as t


@t.no_testdir()
class TEST0(t.BaseTest):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_config_get_alignment', 'notset_fd', 'x')


class TEST1(t.BaseTest):
    test_type = t.Short

    def run(self, ctx):
        size = 16 * t.MiB
        filepath = ctx.create_holey_file(size, 'testfile')
        ctx.exec('pmem2_config_get_alignment', 'get_alignment_success',
                 filepath)


@t.windows_exclude
@t.require_granularity(t.ANY)
class TEST2(t.BaseTest):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_config_get_alignment', 'directory',
                 ctx.testdir)


# XXX must be divided into separate requirements (alignment 4k and 2M)
# at this moment, it can cover only 50% of cases
@t.windows_exclude
@t.require_devdax(t.DevDax('devdax1'))
class TEST3(t.BaseTest):
    test_type = t.Short

    def run(self, ctx):
        dd = ctx.devdaxes.devdax1
        ctx.exec('pmem2_config_get_alignment',
                 'get_alignment_success', dd.path, str(dd.alignment))
