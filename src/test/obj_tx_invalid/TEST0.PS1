# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/obj_tx_invalid/TEST0
#

#
# parameter handling
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 alloc-in-work
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 alloc-in-abort
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 alloc-in-commit
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 alloc-in-finally
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 alloc-after-tx
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 alloc

expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 zalloc-in-work
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 zalloc-in-abort
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 zalloc-in-commit
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 zalloc-in-finally
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 zalloc-after-tx
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 zalloc

expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 strdup-in-work
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 strdup-in-abort
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 strdup-in-commit
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 strdup-in-finally
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 strdup-after-tx
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 strdup

expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 realloc-in-work
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 realloc-in-abort
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 realloc-in-commit
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 realloc-in-finally
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 realloc-after-tx
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 realloc

expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 zrealloc-in-work
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 zrealloc-in-abort
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 zrealloc-in-commit
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 zrealloc-in-finally
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 zrealloc-after-tx
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 zrealloc

expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 free-in-work
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 free-in-abort
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 free-in-commit
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 free-in-finally
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 free-after-tx
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 free

expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 add_range-in-work
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 add_range-in-abort
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 add_range-in-commit
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 add_range-in-finally
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 add_range-after-tx
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 add_range

expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 add_range_direct-in-work
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 add_range_direct-in-abort
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 add_range_direct-in-commit
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 add_range_direct-in-finally
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 add_range_direct-after-tx
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 add_range_direct

expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 abort-in-work
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 abort-in-abort
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 abort-in-commit
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 abort-in-finally
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 abort-after-tx
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 abort

expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 commit-in-work
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 commit-in-abort
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 commit-in-commit
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 commit-in-finally
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 commit-after-tx
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 commit

expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 end-in-work
expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 end-in-abort
expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 end-in-commit
expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 end-in-finally
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 end-after-tx
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 end

expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 process-in-work
expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 process-in-abort
expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 process-in-commit
expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 process-in-finally
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 process-after-tx
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 process

expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 begin-in-work
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 begin-in-abort
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 begin-in-commit
expect_abnormal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 begin-in-finally
expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 begin-after-tx
expect_normal_exit $Env:EXE_DIR\obj_tx_invalid$Env:EXESUFFIX $DIR\testfile1 begin

pass
