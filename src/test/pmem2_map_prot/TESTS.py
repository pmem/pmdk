#!../env.py
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

import testframework as t


class PMEM2_MAP_PROT(t.Test):
    test_type = t.Short
    filesize = 16 * t.MiB

    def run(self, ctx):
        filepath = ctx.create_holey_file(self.filesize, 'testfile',)
        ctx.exec('pmem2_map_prot', self.test_case, filepath)


class TEST0(PMEM2_MAP_PROT):
    """
    READ/WRITE protection on file opened in read-write mode - should succeed
    """
    test_case = "test_rw_mode_rw_prot"


class TEST1(PMEM2_MAP_PROT):
    """
    READ/WRITE protection on file opened in read-only mode - should fail
    """
    test_case = "test_r_mode_rw_prot"


class TEST2(PMEM2_MAP_PROT):
    """
    READ protection on file opened in read-write mode - should succeed
    """
    test_case = "test_rw_mode_r_prot"


class TEST3(PMEM2_MAP_PROT):
    """
    READ protection on file opened in read-only mode - should succeed
    """
    test_case = "test_r_mode_r_prot"


# PMEM2_PROT_NONE flag is not supported by the CreateFileMapping function.
# This test on purpose performs an "Invalid write"
# which causes Memcheck to fail.
@t.windows_exclude
@t.require_valgrind_disabled('memcheck')
class TEST4(PMEM2_MAP_PROT):
    """
    NONE protection on file opened in read-write mode - should succeed
    """
    test_case = "test_rw_mode_none_prot"
