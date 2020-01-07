# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# libpmempool_rm/TEST2 -- test pmempool_rm with non-pool files
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

enable_log_append

[System.IO.File]::WriteAllText("$DIR\file.1", "TEST");
[System.IO.File]::WriteAllText("$DIR\file.2", "TEST`n");
[System.IO.File]::WriteAllText("$DIR\file.3", "");
[System.IO.File]::WriteAllText("$DIR\file.4", "NOTAPMEMPOOLSET`n");

check_files $DIR\file.1 $DIR\file.2 $DIR\file.3 $DIR\file.4

# this should remove all the files
expect_normal_exit $Env:EXE_DIR\libpmempool_rm$Env:EXESUFFIX $DIR\file.1 $DIR\file.2 $DIR\file.3 $DIR\file.4

# check if all pool files are removed
check_no_files $DIR\file.1 $DIR\file.2 $DIR\file.3 $DIR\file.4

# this should report errors for all files
expect_normal_exit $Env:EXE_DIR\libpmempool_rm$Env:EXESUFFIX $DIR\file.1 $DIR\file.2 $DIR\file.3 $DIR\file.4
# this should ignore all errors
expect_normal_exit $Env:EXE_DIR\libpmempool_rm$Env:EXESUFFIX -f $DIR\file.1 $DIR\file.2 $DIR\file.3 $DIR\file.4

check

pass
