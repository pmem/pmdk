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
class TEST0(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_config_get_alignment', 'test_notset_fd')


class TEST1(t.Test):
    test_type = t.Short

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile')
        ctx.exec('pmem2_config_get_alignment',
                 'test_get_alignment_success', filepath)


@t.windows_exclude
@t.require_granularity(t.ANY)
class TEST2(t.BaseTest):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_config_get_alignment', 'test_directory',
                 ctx.testdir)


class PMEM2_CONFIG_GET_ALIGNMENT_DEV_DAX(t.Test):
    test_type = t.Short
    test_case = "test_get_alignment_success"

    def run(self, ctx):
        dd = ctx.devdaxes.devdax
        ctx.exec('pmem2_config_get_alignment',
                 self.test_case, dd.path, str(dd.alignment))


@t.windows_exclude
@t.require_devdax(t.DevDax('devdax', alignment=2 * t.MiB))
class TEST3(PMEM2_CONFIG_GET_ALIGNMENT_DEV_DAX):
    pass


@t.windows_exclude
@t.require_devdax(t.DevDax('devdax', alignment=4 * t.KiB))
class TEST4(PMEM2_CONFIG_GET_ALIGNMENT_DEV_DAX):
    pass
