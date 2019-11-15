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

@t.experimental
class PMEM2_CONFIG(t.BaseTest):
    test_type = t.Short

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        ctx.exec('pmem2_config', self.test_case, filepath)

class TEST0(PMEM2_CONFIG):
    """allocation and dealocation of pmem2_config"""
    test_case = "cfg_create_and_delete_valid"

class TEST1(PMEM2_CONFIG):
    """setting a read + write file descriptor in pmem2_config"""
    test_case = "set_rw_fd"

class TEST2(PMEM2_CONFIG):
    """setting a read only file descriptor in pmem2_config"""
    test_case = "set_ro_fd"

class TEST3(PMEM2_CONFIG):
    """resetting file descriptor in pmem2_config"""
    test_case = "set_negative_fd"

@t.windows_exclude
class TEST4(PMEM2_CONFIG):
    """setting invalid (closed) file descriptor in pmem2_config"""
    test_case = "set_invalid_fd"

class TEST5(PMEM2_CONFIG):
    """setting a write only file descriptor in pmem2_config"""
    test_case = "set_wronly_fd"

class TEST6(PMEM2_CONFIG):
    """allocation of pmem2_config in case of missing memory in system"""
    test_case = "alloc_cfg_enomem"

class TEST7(PMEM2_CONFIG):
    """deleting null pmem2_config"""
    test_case = "delete_null_config"
