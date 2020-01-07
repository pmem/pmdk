# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2019, Intel Corporation

#
# src/test/obj_pmalloc_rand_mt/TEST1 -- multithreaded allocator test
#

. ..\unittest\unittest.ps1

require_fs_type pmem
require_test_type medium
setup

require_free_space 3G

$Env:PMEM_IS_PMEM_FORCE=1
expect_normal_exit $Env:EXE_DIR\obj_pmalloc_rand_mt$Env:EXESUFFIX $DIR\testfile 8 1000 260000 1234

pass
