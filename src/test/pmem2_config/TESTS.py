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


@t.require_granularity(t.ANY)
class PMEM2_CONFIG(t.Test):
    test_type = t.Short

    def run(self, ctx):
        filepath = ctx.create_holey_file(16 * t.MiB, 'testfile1')
        ctx.exec('pmem2_config', self.test_case, filepath)


@t.no_testdir()
class PMEM2_CONFIG_NO_DIR(t.Test):
    test_type = t.Short

    def run(self, ctx):
        ctx.exec('pmem2_config', self.test_case, 'unused')


class TEST0(PMEM2_CONFIG_NO_DIR):
    """allocation and dealocation of pmem2_config"""
    test_case = "cfg_create_and_delete_valid"


class TEST1(PMEM2_CONFIG):
    """setting a read + write file descriptor in pmem2_config"""
    test_case = "set_rw_fd"


class TEST2(PMEM2_CONFIG):
    """setting a read only file descriptor in pmem2_config"""
    test_case = "set_ro_fd"


class TEST3(PMEM2_CONFIG_NO_DIR):
    """resetting file descriptor in pmem2_config"""
    test_case = "set_negative_fd"


class TEST4(PMEM2_CONFIG):
    """setting invalid (closed) file descriptor in pmem2_config"""
    test_case = "set_invalid_fd"


class TEST5(PMEM2_CONFIG):
    """setting a write only file descriptor in pmem2_config"""
    test_case = "set_wronly_fd"


class TEST6(PMEM2_CONFIG_NO_DIR):
    """allocation of pmem2_config in case of missing memory in system"""
    test_case = "alloc_cfg_enomem"


class TEST7(PMEM2_CONFIG_NO_DIR):
    """deleting null pmem2_config"""
    test_case = "delete_null_config"


class TEST8(PMEM2_CONFIG_NO_DIR):
    """set valid granularity in the config"""
    test_case = "config_set_granularity_valid"


class TEST9(PMEM2_CONFIG_NO_DIR):
    """set invalid granularity in the config"""
    test_case = "config_set_granularity_invalid"


@t.windows_only
class TEST10(PMEM2_CONFIG):
    """set handle in the config"""
    test_case = "set_handle"


@t.windows_only
class TEST11(PMEM2_CONFIG_NO_DIR):
    """set INVALID_HANLE_VALUE in the config"""
    test_case = "set_null_handle"


@t.windows_only
class TEST12(PMEM2_CONFIG):
    """set invalid handle in the config"""
    test_case = "set_invalid_handle"


@t.windows_only
class TEST13(PMEM2_CONFIG):
    """set handle to a directory in the config"""
    test_case = "set_directory_handle"

    def run(self, ctx):
        ctx.exec('pmem2_config', self.test_case, ctx.testdir)


@t.windows_only
class TEST14(PMEM2_CONFIG_NO_DIR):
    """set handle to a mutex in the config"""
    test_case = "set_mutex_handle"


@t.windows_exclude
class TEST15(PMEM2_CONFIG):
    """set directory's fd in the config"""
    test_case = "set_directory_fd"

    def run(self, ctx):
        ctx.exec('pmem2_config', self.test_case, ctx.testdir)


class TEST16(PMEM2_CONFIG_NO_DIR):
    """setting offset which is too large"""
    test_case = "set_offset_too_large"


class TEST17(PMEM2_CONFIG_NO_DIR):
    """setting a valid offset"""
    test_case = "set_offset_success"


class TEST18(PMEM2_CONFIG_NO_DIR):
    """setting a valid length"""
    test_case = "set_length_success"


class TEST19(PMEM2_CONFIG_NO_DIR):
    """setting maximum possible offset"""
    test_case = "set_offset_max"
